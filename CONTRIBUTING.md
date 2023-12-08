# Contributing to Bitcoin Cash Node

The Bitcoin Cash Node project welcomes contributors!

This guide is intended to help developers and others contribute effectively
to Bitcoin Cash Node.

## Communicating with the project

To get in contact with the Bitcoin Cash Node project, we monitor a number
of resources.

Our main development repository is currently located at

[https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node](https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node)

This features the project code, an issue tracker and facilities to see
project progress and activities, even in detailed form such as individual
change requests.

Users are free to submit issues or comment on existing ones - all that is
needed is a GitLab account which can be freely registered (use the 'Register'
button on the GitLab page).

In addition to the project repository, we have various other channels where
project contributors can be reached.

Our main chat is at <https://bitcoincashnode.slack.com>, where we conduct
our main development and interactive support for users of our node.

Other social media resources such as our Telegram and Twitter are linked
from the project website at

[https://bitcoincashnode.org](https://bitcoincashnode.org)

On all our channels, we seek to facilitate development of Bitcoin Cash Node,
and to welcome and support people who wish to participate.

Please visit our channels to

- Introduce yourself to other Bitcoin Cash Node contributors
- Get help with your development environment
- Discuss how to complete a patch.

It is not for:

- Market discussion
- Non-constructive criticism

## Bitcoin Cash Node Development Philosophy

Bitcoin Cash Node aims for fast iteration and continuous integration.

This means that there should be quick turnaround for patches to be proposed,
reviewed, and committed. Changes should not sit in a queue for long.

### Expectations of contributors

Here are some tips to help keep the development working as intended. These
are guidelines for the normal and expected development process. Developers
can use their judgement to deviate from these guidelines when they have a
good reason to do so.

- Keep each change minimal and self-contained.
- Don't amend a Merge Request after it has been accepted for merging unless
  with coordination with the maintainer(s)
- Large changes should be broken into logical chunks that are easy to review,
  and keep the code in a functional state.
- Do not mix moving stuff around with changing stuff. Do changes with renames
  on their own.
- Sometimes you want to replace one subsystem by another implementation,
  in which case it is not possible to do things incrementally. In such cases,
  you keep both implementations in the codebase for a while, as described
  [here](https://www.gamasutra.com/view/news/128325/Opinion_Parallel_Implementations.php)
- There are no "development" branches, all merge requests apply to the master
  branch, and should always improve it (no regressions).
- As soon as you see a bug, you fix it. Do not continue on. Fixing the bug
  becomes the top priority, more important than completing other tasks.

Note: Code review is a burdensome but important part of the development process,
and as such, certain types of merge requests are rejected. In general, if the
improvements do not warrant the review effort required, the MR has a high
chance of being rejected. Before working on a large or complex code change,
it is recommended to consult with project maintainers about the desirability
of the change, so you can be sure they are willing to spend the time required
to review your work.

### Merge request philosophy

Patchsets should always be focused. For example, a merge request could add a
feature, fix a bug, or refactor code; but not a mixture. Please also avoid super
merge requests which attempt to do too much, are overly large, or overly complex
as this makes review difficult.

#### Features

When adding a new feature, thought must be given to the long term technical debt
and maintenance that feature may require after inclusion. Before proposing a new
feature that will require maintenance, please consider if you are willing to
maintain it (including bug fixing). If features get orphaned with nobody using
and caring for them in the future, they may be removed by the project maintainers.

#### Refactoring

Refactoring is a necessary part of any software project's evolution. The
following guidelines cover refactoring merge requests for the project.

There are three categories of refactoring: code-only moves, code style fixes, and
code refactoring. In general, refactoring merge requests should not mix these
three kinds of activities in order to make refactoring merge requests easy to
review and uncontroversial. In all cases, refactoring MRs must not change the
behaviour of code within the merge request (bugs must be preserved as is).

Project maintainers aim for a quick turnaround on refactoring merge requests, so
where possible keep them short, uncomplex and easy to verify.

Merge requests that refactor the code should not be made by new contributors. It
requires a certain level of experience to know where the code belongs and to
understand the full ramification (including rebase effort of other open MRs).

Trivial merge requests or merge requests that refactor the code with no clear
benefits may be immediately closed by the maintainers to reduce unnecessary
workload on reviewing.

#### Critical code paths

Some code paths are more critical than others. This applies to code such as
consensus, block template creation, mempool, relay policies and more. *Changes
to critical code paths really should inspire confidence*. The following things
can help inspire confidence:

- Minimal, easily reviewable merge requests.
- Extensive unit and/or functional tests.
- Patience for reviewers
- Appreciation for reviews.

#### Approval of paid merge requests

1. Contributors are welcome to submit merge requests, but they should not
   expect to be paid for MRs in general, unless...

2. If a contributor expects to be paid for an MR, they should get explicit
   approval from maintainers before conducting the work. The approval must
   be documented directly in the issue or MR in the form of maximum
   approved hours for the work. For example `<Developer> approved for
   maximum 5 hours on this MR.` In line with maintainers' discretion to
   close any MR when it does not meet the needs of the project, approval
   for pay may also be revoked if the work deviates from expectations.

3. Regular contributors in good standing may be informed by maintainers of
   their trusted status, which grants them the privilege to start paid work
   without prior approval each time. This status may be revoked at the
   discretion of the maintainers.

### Expectations of maintainers

These are guidelines for the normal and expected process for handling merge
requests. Maintainers can use their judgement to deviate from these guidelines
when they have a good reason to do so.

- Try to guide contributors towards the goal of having their contributions
  merged.
- Identify whether the intent of an MR is desirable, independent of its
  technical quality.
- Merge accepted changes quickly after they are accepted.
- If a merge has been done and breaks the build of master, fix it quickly. If
  it cannot be fixed quickly, revert it. It can be re-applied later when it no
  longer breaks the build.
- Automate as much as possible, and spend time on things only humans can do.
- Speak up or raise an issue if you anticipate a problem with a change.
- Don't be afraid to say "NO", or "MAYBE, but...", if a change seems
  undesirable or if you otherwise have reservations/caveats/etc.

Here are some handy links for development practices aligned with Bitcoin Cash Node:

- [BCHN GitLab development working rules and guidelines](doc/bchn-gitlab-usage-rules-and-guidelines.md)
- [Developer Notes](doc/developer-notes.md)
- [How to Write a Git Commit Message](https://chris.beams.io/posts/git-commit/)
- [How to Do Code Reviews Like a Human - Part 1](https://mtlynch.io/human-code-reviews-1/)
- [How to Do Code Reviews Like a Human - Part 2](https://mtlynch.io/human-code-reviews-2/)
- [Large Diffs Are Hurting Your Ability To Ship](https://medium.com/@kurtisnusbaum/large-diffs-are-hurting-your-ability-to-ship-e0b2b41e8acf)
- [Parallel Implementations](https://www.gamasutra.com/view/news/128325/Opinion_Parallel_Implementations.php)
- [The Pragmatic Programmer: From Journeyman to Master](https://www.amazon.com/Pragmatic-Programmer-Journeyman-Master/dp/020161622X)
- [Advantages of monolithic version control](https://danluu.com/monorepo/)
- [The importance of fixing bugs immediately](https://youtu.be/E2MIpi8pIvY?t=16m0s)
- [Good Work, Great Work, and Right Work](https://forum.dlang.org/post/q7u6g1$94p$1@digitalmars.com)
- [Accelerate: The Science of Lean Software and DevOps](https://www.amazon.com/Accelerate-Software-Performing-Technology-Organizations/dp/1942788339)

## Getting set up with the Bitcoin Cash Node Repository

1. Create an account at [https://gitlab.com](https://gitlab.com) if you don't have
   one yet
2. Install Git on your machine

    - Git documentation can be found at: [https://git-scm.com](https://git-scm.com)
    - To install these packages on Debian or Ubuntu,
      type: `sudo apt-get install git`

3. If you do not already have an SSH key set up, follow these steps:

    - Type: `ssh-keygen -t rsa -b 4096 -C "your_email@example.com"`
    - Enter a file in which to save the key
      (/home/*username*/.ssh/id_rsa): [Press enter]
    - *NOTE: the path in the message shown above is specific to UNIX-like systems
      and may differ if you run on other platforms.*

4. Upload your SSH public key to GitLab

    - Go to: [https://gitlab.com](https://gitlab.com), log in
    - Under "User Settings", "SSH Keys", add your public key
    - Paste contents from: `$HOME/.ssh/id_rsa.pub`

5. Create a personal fork of the Bitcoin Cash Node repository for your work

    - Sign into GitLab under your account, then visit the project at [https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node](https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node)
    - Click the 'Fork' button on the top right, and choose to fork the project to
      your personal GitLab space.

6. Clone your personal work repository to your local machine:

    ```
    git clone git@gitlab.com:username/bitcoin-cash-node.git
    ```

7. Set your checked out copy's upstream to our main project:

    ```
    git remote add upstream https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node.git
    ```

8. You may want to add the `mreq` alias to your `.git/config`:

    ```
    [alias]
    mreq = !sh -c 'git fetch $1 merge-requests/$2/head:mr-$1-$2 && git checkout mr-$1-$2' -
    ```

    This `mreq` alias can be used to easily check out Merge Requests from our
    main repository if you intend to test them or work on them.
    Example:

    ```
    $ git mreq upstream 93
    ```

    This will checkout `merge-requests/93/head` and put you in a branch called `mr-origin-93`.
    You can then proceed to test the changes proposed by that merge request.

9. Code formatting tools

    During submission of patches, our GitLab CI process may refuse code that
    is not styled according to our coding guidelines.

    To enforce Bitcoin Cash Node codeformatting standards, you may need to
    install `clang-format-11`, `clang-tidy` (version >=11), `autopep8`, `flake8`,
    `phpcs` and `shellcheck` on your system to format your code before submission
    as a Merge Request.

    To install `clang-format-11` and `clang-tidy` on Ubuntu (>= 18.04+updates)
    or Debian (>= 10):

    ```
    sudo apt-get install clang-format-11 clang-tidy-11 clang-tools-11
    ```

    If not available in the distribution, `clang-format-11` and `clang-tidy` can be
    installed from [https://releases.llvm.org/download.html](https://releases.llvm.org/download.html)
    or [https://apt.llvm.org](https://apt.llvm.org).

    For example, for macOS:

    ```
    curl -L https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/clang+llvm-11.0.0-x86_64-apple-darwin.tar.xz | tar -xJv
    ln -s $PWD/clang+llvm-11.0.0-x86_64-apple-darwin/bin/clang-format /usr/local/bin/clang-format
    ln -s $PWD/clang+llvm-11.0.0-x86_64-apple-darwin/bin/clang-tidy /usr/local/bin/clang-tidy
    ```

    To install `autopep8`, `flake8`, `phpcs` and `shellcheck` on Ubuntu:

    ```
    sudo apt-get install python-autopep8 flake8 php-codesniffer shellcheck
    ```

10. Further task-specific dependencies

    In order to run the automated tests, you'll need the tools listed in the "dependencies" sections of these two documents:

    - [Linting](doc/linting.md)
    - [Functional Tests](doc/functional-tests.md)

    To produce coverage reports, you'll need the dependencies listed in:

    - [Coverage](doc/coverage.md)

    To run HTML documenation generation locally, you'll need the `mkdocs` and `mkdocs-material` Python packages as covered in:

    - [Publishing documentation](doc/publishing-documentation.md)

## Working with The Bitcoin Cash Node Repository

A typical workflow would be:

- Create a topic branch in Git for your changes

    `git checkout -b 'my-topic-branch'`

- Make your changes, and commit them

    `git commit -a -m 'my-commit'`

- Push the topic branch to your GitLab repository

    `git push -u origin my-topic-branch`

- Then create a Merge Request (the GitLab equivalent of a Pull Request)
  from that branch in your personal repository. To do this, you need to
  sign in to GitLab, go to the branch and click the button which lets you
  create a Merge Request (you need to fill out at least title and description
  fields).

- Work with us on GitLab to receive review comments, going back to the
  'Make your changes' step above if needed to make further changes on your
  branch, and push them upstream as above. They will automatically appear
  in your Merge Request.

All Merge Requests should contain a commit with a test plan that details
how to test the changes. In all normal circumstances, you should build and
test your changes locally before creating a Merge Request.
A merge request should feature a specific test plan where possible, and
indicate which regression tests may need to be run.

If you have doubts about the scope of testing needed for your change,
please contact our developers and they will help you decide.

- For large changes, break them into several Merge Requests.

- If you are making numerous changes and rebuilding often, it's highly
  recommended to install `ccache` (re-run cmake if you install it
  later), as this will help cut your re-build times from several minutes to under
  a minute, in many cases.

## What to work on

If you are looking for a useful task to contribute to the project, a good place
to start is the list of issues at [https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node/-/issues](https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node/-/issues)

Look for issues marked with a label 'good-first-issue'.

## Bitcoin Cash Node documentation

Find the complete project documentation at [https://docs.bitcoincashnode.org/](https://docs.bitcoincashnode.org/).
The documentation here is published automatically as part of the development
release pipeline. See [Publishing documentation](doc/publishing-documentation.md)
for further details and especially if you plan to work on the documentation.

## Copyright

By contributing to this repository, you agree to license your work under the
MIT license unless specified otherwise in `contrib/debian/copyright` or at
the top of the file itself. Any work contributed where you are not the original
author must contain its license header with the original author(s) and source.

## Disclosure Policy

See [DISCLOSURE_POLICY](DISCLOSURE_POLICY.md).
