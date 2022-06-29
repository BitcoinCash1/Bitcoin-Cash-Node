// Copyright (c) 2015-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <scheduler.h>

#include <random.h>
#include <reverselock.h>

#include <cassert>
#include <utility>

CScheduler::CScheduler()
    : nThreadsServicingQueue(0), stopRequested(false), stopWhenEmpty(false) {}

CScheduler::~CScheduler() {
    assert(nThreadsServicingQueue == 0);
}

void CScheduler::serviceQueue() {
    std::unique_lock lock(newTaskMutex);
    ++nThreadsServicingQueue;

    // newTaskMutex is locked throughout this loop EXCEPT when the thread is
    // waiting or when the user's function is called.
    while (!shouldStop()) {
        try {
            if (!shouldStop() && taskQueue.empty()) {
                reverse_lock rlock(lock);
                // Use this chance to get more entropy
                RandAddSeedSleep();
            }
            while (!shouldStop() && taskQueue.empty()) {
                // Wait until there is something to do.
                newTaskScheduled.wait(lock);
            }

            // Wait until either there is a new task, or until the time of the
            // first item on the queue.
            // Some boost versions have a conflicting overload of wait_until
            // that returns void. Explicitly use a template here to avoid
            // hitting that overload.
            while (!shouldStop() && !taskQueue.empty()) {
                std::chrono::system_clock::time_point timeToWaitFor =
                    taskQueue.begin()->first;
                if (newTaskScheduled.wait_until<>(lock, timeToWaitFor) ==
                    std::cv_status::timeout) {
                    // Exit loop after timeout, it means we reached the
                    // time of the event
                    break;
                }
            }

            // If there are multiple threads, the queue can empty while we're
            // waiting (another thread may service the task we were waiting on).
            if (shouldStop() || taskQueue.empty()) {
                continue;
            }

            Function f = taskQueue.begin()->second;
            taskQueue.erase(taskQueue.begin());

            {
                // Unlock before calling f, so it can reschedule itself or
                // another task without deadlocking:
                reverse_lock rlock(lock);
                f();
            }
        } catch (...) {
            --nThreadsServicingQueue;
            throw;
        }
    }
    --nThreadsServicingQueue;
    newTaskScheduled.notify_one();
}

void CScheduler::stop(bool drain) {
    {
        std::unique_lock lock(newTaskMutex);
        if (drain) {
            stopWhenEmpty = true;
        } else {
            stopRequested = true;
        }
    }
    newTaskScheduled.notify_all();
}

void CScheduler::schedule(CScheduler::Function f,
                          std::chrono::system_clock::time_point t) {
    {
        std::unique_lock lock(newTaskMutex);
        taskQueue.insert(std::make_pair(t, f));
    }
    newTaskScheduled.notify_one();
}

void CScheduler::scheduleFromNow(CScheduler::Function f,
                                 int64_t deltaMilliSeconds) {
    schedule(f, std::chrono::system_clock::now() + std::chrono::milliseconds(deltaMilliSeconds));
}

void CScheduler::MockForward(std::chrono::seconds delta_seconds) {
    assert(delta_seconds.count() > 0 &&
           delta_seconds < std::chrono::hours{1});

    {
        std::unique_lock lock(newTaskMutex);

        // use temp_queue to maintain updated schedule
        std::multimap<std::chrono::system_clock::time_point, Function>
            temp_queue;

        for (const auto &element : taskQueue) {
            temp_queue.emplace_hint(temp_queue.cend(),
                                    element.first - delta_seconds,
                                    element.second);
        }

        // point taskQueue to temp_queue
        taskQueue = std::move(temp_queue);
    }

    // notify that the taskQueue needs to be processed
    newTaskScheduled.notify_one();
}

static void Repeat(CScheduler *s, CScheduler::Predicate p,
                   int64_t deltaMilliSeconds) {
    if (p()) {
        s->scheduleFromNow(std::bind(&Repeat, s, p, deltaMilliSeconds),
                           deltaMilliSeconds);
    }
}

void CScheduler::scheduleEvery(CScheduler::Predicate p,
                               int64_t deltaMilliSeconds) {
    scheduleFromNow(std::bind(&Repeat, this, p, deltaMilliSeconds),
                    deltaMilliSeconds);
}

size_t
CScheduler::getQueueInfo(std::chrono::system_clock::time_point &first,
                         std::chrono::system_clock::time_point &last) const {
    std::unique_lock lock(newTaskMutex);
    size_t result = taskQueue.size();
    if (!taskQueue.empty()) {
        first = taskQueue.begin()->first;
        last = taskQueue.rbegin()->first;
    }
    return result;
}

bool CScheduler::AreThreadsServicingQueue() const {
    std::unique_lock lock(newTaskMutex);
    return nThreadsServicingQueue;
}

void SingleThreadedSchedulerClient::MaybeScheduleProcessQueue() {
    {
        LOCK(m_cs_callbacks_pending);
        // Try to avoid scheduling too many copies here, but if we
        // accidentally have two ProcessQueue's scheduled at once its
        // not a big deal.
        if (m_are_callbacks_running) {
            return;
        }
        if (m_callbacks_pending.empty()) {
            return;
        }
    }
    m_pscheduler->schedule(
        std::bind(&SingleThreadedSchedulerClient::ProcessQueue, this));
}

void SingleThreadedSchedulerClient::ProcessQueue() {
    std::function<void()> callback;
    {
        LOCK(m_cs_callbacks_pending);
        if (m_are_callbacks_running) {
            return;
        }
        if (m_callbacks_pending.empty()) {
            return;
        }
        m_are_callbacks_running = true;

        callback = std::move(m_callbacks_pending.front());
        m_callbacks_pending.pop_front();
    }

    // RAII the setting of fCallbacksRunning and calling
    // MaybeScheduleProcessQueue to ensure both happen safely even if callback()
    // throws.
    struct RAIICallbacksRunning {
        SingleThreadedSchedulerClient *instance;
        explicit RAIICallbacksRunning(SingleThreadedSchedulerClient *_instance)
            : instance(_instance) {}
        ~RAIICallbacksRunning() {
            {
                LOCK(instance->m_cs_callbacks_pending);
                instance->m_are_callbacks_running = false;
            }
            instance->MaybeScheduleProcessQueue();
        }
    } raiicallbacksrunning(this);

    callback();
}

void SingleThreadedSchedulerClient::AddToProcessQueue(
    std::function<void()> func) {
    assert(m_pscheduler);

    {
        LOCK(m_cs_callbacks_pending);
        m_callbacks_pending.emplace_back(std::move(func));
    }
    MaybeScheduleProcessQueue();
}

void SingleThreadedSchedulerClient::EmptyQueue() {
    assert(!m_pscheduler->AreThreadsServicingQueue());
    bool should_continue = true;
    while (should_continue) {
        ProcessQueue();
        LOCK(m_cs_callbacks_pending);
        should_continue = !m_callbacks_pending.empty();
    }
}

size_t SingleThreadedSchedulerClient::CallbacksPending() {
    LOCK(m_cs_callbacks_pending);
    return m_callbacks_pending.size();
}
