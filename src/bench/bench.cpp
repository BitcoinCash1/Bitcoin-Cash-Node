// Copyright (c) 2015-2016 The Bitcoin Core developers
// Copyright (c) 2017-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>

#include <chain.h>
#include <chainparams.h>
#include <validation.h>

#include <test/setup_common.h>

#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <regex>

namespace benchmark {

void ConsolePrinter::header() {
    std::cout << "# Benchmark, evals, iterations, total, min, max, median"
              << std::endl;
}

void ConsolePrinter::result(const State &state, uint64_t num_evals) {
    auto results = state.m_elapsed_results;
    std::sort(results.begin(), results.end());

    double total = state.m_num_iters *
                   std::accumulate(results.begin(), results.end(), 0.0);

    double min    = 0;
    double max    = 0;
    double median = 0;

    if (!results.empty()) {
        min = results.front();
        max = results.back();

        size_t mid = results.size() / 2;
        median = results.size() % 2 ? results[mid] : (results[mid - 1] + results[mid]) / 2.0;
    }

    std::cout << std::setprecision(6)
              << state.m_name << ", " << num_evals << ", "
              << state.m_num_iters << ", " << total << ", " << min << ", "
              << max << ", " << median << std::endl;
}

void ConsolePrinter::footer() {}
PlotlyPrinter::PlotlyPrinter(const std::string &plotly_url,
                             int64_t width,
                             int64_t height)
    : m_plotly_url(plotly_url), m_width(width), m_height(height) {}

void PlotlyPrinter::header() {
    std::cout << "<html><head>"
              << "<script src=\"" << m_plotly_url << "\"></script>"
              << "</head><body><div id=\"myDiv\" style=\"width:" << m_width
              << "px; height:" << m_height << "px\"></div>"
              << "<script> var data = [" << std::endl;
}

void PlotlyPrinter::result(const State &state, uint64_t num_evals) {
    std::cout << "{ " << std::endl
              << "  name: '" << state.m_name << "', " << std::endl
              << "  y: [";

    const char *prefix = "";
    for (const auto &e : state.m_elapsed_results) {
        std::cout << prefix << std::setprecision(6) << e;
        prefix = ", ";
    }
    std::cout << "]," << std::endl
              << "  boxpoints: 'all', jitter: 0.3, pointpos: 0, type: 'box',"
              << std::endl
              << "}," << std::endl;
}

void PlotlyPrinter::footer() {
    std::cout << "]; var layout = { showlegend: false, yaxis: { rangemode: "
                 "'tozero', autorange: true } };"
              << "Plotly.newPlot('myDiv', data, layout);"
              << "</script></body></html>";
}

BenchRunner::BenchmarkMap &BenchRunner::benchmarks() {
    static std::map<std::string, Bench> benchmarks_map;
    return benchmarks_map;
}

BenchRunner::BenchRunner(const std::string &name,
                         BenchFunction func,
                         uint64_t num_iters_for_one_second) {
    benchmarks().insert(
        std::make_pair(name, Bench{func, num_iters_for_one_second}));
}

void BenchRunner::RunAll(Printer &printer, uint64_t num_evals,
                         double scaling, const std::string &filter,
                         bool is_list_only) {
    if (!std::ratio_less_equal<clock::period, std::micro>::value) {
        std::cerr << "WARNING: Clock precision is worse than microsecond - "
                     "benchmarks may be less accurate!\n";
    }
#ifdef DEBUG
    std::cerr << "WARNING: This is a debug build - may result in slower "
                 "benchmarks.\n";
#endif

    std::regex reFilter(filter);
    std::smatch baseMatch;

    printer.header();

    for (const auto &[name, bench] : benchmarks()) {
        if (!std::regex_match(name, baseMatch, reFilter)) {
            continue;
        }

        uint64_t const num_iters = std::max(static_cast<uint64_t>(bench.num_iters_for_one_second * scaling), uint64_t{1});

        if (is_list_only) {
            std::cout << name << ", " << num_evals << ", " << num_iters << std::endl;
            continue;
        }

        State state(name, num_iters, printer);
        for (uint64_t i = 0; i != num_evals; ++i) {
            TestingSetup test{CBaseChainParams::REGTEST};
            assert(::ChainActive().Height() == 0);

            state.m_num_iters_left = state.m_num_iters;
            state.m_start_time = time_point(); // we expect each benchmark to set this at the entry to its loop, after setup, so we use default value to assert it has been done

            bench.func(state);
        }
        printer.result(state, num_evals);
    }

    printer.footer();
}

void State::UpdateTimer(const time_point &current_time) {
    std::chrono::duration<double> const diff = current_time - m_start_time;
    m_elapsed_results.push_back(diff.count() / m_num_iters);
}

void
#if defined(__clang__)
__attribute__((optnone))
#elif defined(__GNUC__)
__attribute__((optimize(0),no_reorder))
#endif
internal::NoOptimize(...) noexcept
{
#ifdef __GNUC__ // NB: this also is true for clang
    __asm__ __volatile__ ("":::"memory"); // acts as a memory barrier and may prevent some forms of LTO and reordering.
#endif
}

} // namespace benchmark
