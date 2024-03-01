# Libauth CHIP test expected failure reasons

## Background: The difficulty of checking Libauth test errors

For most automated unit tests, not only are the test success values checked, but the resulting error messages are too.
Potentially significant changes to internal logic that basic pass/fail checks would miss can be caught by this simple
additional check.

In the usual case, developing tests for this is straightforward since the developer need only record the error message
along with the pass/fail state when running tests under different conditions.

| Test    | Testing context 1 | Testing context 2 | Testing context 3 |
| ------- | ----------------- | ----------------- | ----------------- |
| Test 1: | pass              | pass              | pass              |
| Test 2: | fail: "Error 1"   | fail: "Error 2"   | pass              |
| Test 3: | fail: "Error 1"   | pass              | fail: "Error 3"   |

BCHN directly imports certain
[Libauth cross-platform transaction tests](https://github.com/bitauth/libauth/tree/master/src/lib/vmb-tests#readme)
for testing CHIPs, which poses a complication. These tests are externally supplied and as a result the error messages
expected from BCHN are not recorded/assigned to the tests; since the error messages are platform specific. Rather than
ignore the error messages for these tests, we develop our own list of expected error messages for each test.

On the face of it - as with the rest of our automated tests - we would need to simply run each test, check that the
results make sense, and record the results in a list of expected results for future test runs to consult. However, since
there are thousands of individual Libauth tests, likely to be many more and potentially many modifications, the human
effort to accomplish this would be quite significant and prone to human error.

Thankfully, each Libauth test comes with a "reason" message for each expected test failure which can help reduce the
manual work required here. If each provided reason happened to map one-to-one with an expected BCHN error message, then
we need only build a basic lookup table. But this is not the case. There are instances where Libauth suggests rather
specific failure reasons where BCHN gives only general error messages, for example, and vice versa.

## Solution

To address this, we prepare a lookup table with the most common mapping between Libauth suggested reasons and
BCHN errors, with overrides provided for specific tests. This is broken up by testing context (otherwise there would be
so much inconsistency that overrides would outnumber rules). Testing context here means; which CHIP is being tested,
whether or not the CHIP is pre- or post-activation in the testing environment, and whether standard or non-standard
validation is enabled. Those context permutations are represented as numbers in the table below for simplicity. Also for
simplicity, there is only a single rule for each Libauth reason. There are no rules covering more specific contexts that
override more general rules. Only overrides do this, and overrides always target a specific context.

This allows for many matches to be captured by general rules while allowing for a minimized set of test-specific
overrides for odd cases.

| Rule or Override | Libauth suggested reason | Testing contexts | Override test IDs | BCHN error message |
| ---------------- | ------------------------ | ---------------- | ----------------- | ------------------ |
| Rule             | "Reason 1"               | all              |                   | "Error 1"          |
| Override         | "Reason 1"               | 2                | id5, id2          | "Error 2"          |
| Rule             | "Reason 2"               | 1 & 2            |                   | "Error 1"          |
| Override         | "Reason 2"               | 1                | id1               | "Error 5"          |
| Override         | "Reason 2"               | 2                | id3, id4, id6     | "Error 3"          |
| Rule             | "Reason 3"               | all              |                   | "Error 4"          |

With this lookup table in place, the process to determine the expected BCHN error message from a Libauth test is simply
to find an override matching this test’s ID and testing context, and if not found, find a rule matching the test’s
suggested reason and testing context.

## Manual oversight process

This table is generated automatically when the Libauth CHIP tests are run. As with normal test development, the produced
BCHN error messages need to be manually checked to ensure they make sense given the context. To help with this, a
special human-readable checklist is automatically generated that includes more context information and individual test
details alongside each rule/override. If going from scratch, all of the BCHN error messages in each row need to be
manually checked to ensure they make sense given the context. However, once this is done, any subsequent additions or
modifications to the Libauth CHIP tests are automatically marked up in the checklist as "new", allowing the reviewer to
avoid rechecking rules that have already been confirmed.

The lookup table itself is stored in a JSON file: `expected_test_fail_reasons.json` and the human-readable checklist is
stored in a CSV file: `expected_reasons_checklist.csv`. These are written to the current directory from where
`test_bitcoin` is run. For instance, if running locally from your build directory with
`src/test/test_bitcoin –run_test=libauth_chip_tests` then the two files will appear in your build directory. Note; these
are only output if the lookup table does not exactly match the expected lookup table. To force generation, just make a
minor edit to `expected_test_fail_reasons.json`, such as adding a fake override entry, then recompile and rerun the
tests.

While working your way through the checklist, the task is simply to confirm that each entry in the "BCHN error
message" column makes sense given the row's "Libauth suggested reason" and testing context.  If any match seems
suspect, check details of the specific tests that the rule applies to, which are given in the last columns of each row.
If any match is wrong, fix the bug with the associated tests before proceeding.

Once confirming that every new match makes sense, simply copy the generated `expected_test_fail_reasons.json` file into
`src/test/data/`, overwriting the original.  Finally, recompile and rerun the tests to confirm the tests now pass.
