Libauth regression and CHIP tests. The tests in this directory come from here:

https://github.com/bitjson/libauth/tree/d0d54b99445af46176804ec9a43c18a1edb9f139/src/lib/vmb-tests/generated

See the subdirectory CHIPs/ for the CHIP tests.  (CHIP tests also test activation, whereas regression tests
do not).

---

Note about the `*_scriptonly.json` files: These are manually-created files that contain lists of tests to run in
"script only" mode.  This mode is used to skip the AcceptToMemoryPool() code-path and instead only do minimal txn
checks and then evaluate the input in question using the script VM.

The reason for this mode is because some of the Libauth tests we imported are doing unorthodox things like setting tx
locktime to crazy values, or moving around more BCH than MAX_MONEY (21 million). So they would normally fail in the ATMP
code path, but we want them to succeed since some of these tests are really concerned with testing VM opcodes moreso
than other consensus rules. So, this "script only" mode is less restrictive and really just tests the script VM mostly,
and not the full ATMP path.
