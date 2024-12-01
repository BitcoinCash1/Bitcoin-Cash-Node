# Release Notes for Bitcoin Cash Node version 28.0.0

Bitcoin Cash Node version 28.0.0 is now available from:

  <https://bitcoincashnode.org>

## Overview

This is a major release of Bitcoin Cash Node (BCHN) that implements the May 15, 2025 Network Upgrade. The upgrade
implements the following two consensus-level CHIPs:

- [CHIP-2021-05 VM Limits: Targeted Virtual Machine Limits](https://github.com/bitjson/bch-vm-limits/tree/master)
- [CHIP-2024-07 BigInt: High-Precision Arithmetic for Bitcoin Cash](https://github.com/bitjson/bch-bigint)

Additionally, this version contains various other minor corrections and improvements.

Users who are running any of our previous releases (v27.x.x or before) are urged to upgrade to v28.0.0 ahead of
May 15, 2025.

## Usage recommendations

The update to Bitcoin Cash Node 28.0.0 is required for the May 15, 2025 Bitcoin Cash network upgrade.

## Network changes

- UPnP: If there is an error opening the port, BCHN will now keep retrying periodically until it succeeds, rather than
  simply failing and giving up.
- p2p protocol: BCHN now ignores timestamps from inbound peers for the purposes of calculating network adjusted
  time.
- p2p protocol: BCHN now ignores peer timestamps that seem bogus/malformed/garbage/unlikely.
- HTTP server: Set TCP_NODELAY on the server socket, which should improve RPC and REST server responsiveness.

## Added functionality

- Added NAT-PMP port mapping support via [`libnatpmp`](https://miniupnp.tuxfamily.org/libnatpmp.html).
  Use the `-natpmp` command line option to use NAT-PMP to map the listening port. If both UPnP
  and NAT-PMP are enabled, a successful allocation from UPnP prevails over one from NAT-PMP.
- bitcoin-seeder now saves its current state to the dump and db files immediately before exiting when gracefully
  shut down via Ctrl-C, SIGINT, SIGTERM, etc.


## Deprecated functionality

None

## Modified functionality

- bitcoin-seeder now refuses to start if it cannot bind to the DNS port specified, rather than silently suceeding.
- The transaction script execution engine has been greatly extended and expanded. These changes include:
    - Various execution limits for the Script VM have been greatly expanded and extended. For a full description,
      of what has been added and improved, see: https://github.com/bitjson/bch-vm-limits/tree/master.
    - Support for arbitrary precision integers has been added to the Script VM. The previous limit was 64-bit integers,
      now the integer limit has been expanded to support up to 80,000-bit integers.
    - Both of the above-mentioned consensus changes will activate on the mainnet network on May 15, 2025.

## Removed functionality

Support for 32-bit architectures (such as armhf and linux-i686) has been completely dropped. Due to the limitations
in available virtual memory on 32-bit, and the increasing demands of Bitcoin Cash to support large blocks, 32-bit
architectures are no longer supported by Bitcoin Cash Node. We apologize for the inconvenience but are hopeful that
this change in requirements will have minimal to zero impact on our users, most of whom are likely already using a
64-bit platform.

## New RPC methods

- The `getindexinfo` RPC returns the actively running indices of the node,
  including their current sync status and height. It also accepts an `index_name`
  to specify returning only the status of that index.

## User interface changes

- RPC: Added input `tokenData` and `scriptPubKey` to RPC method `getrawtransaction` with `verbosity=2`

## Regressions

Bitcoin Cash Node 28.0.0 does not introduce any known regressions as compared to 27.1.0 and/or 27.0.0.

## Limitations

The following are limitations in this release of which users should be aware:

1. CashToken support is low-level at this stage. The wallet application does
   not yet keep track of the user's tokens.
   Tokens are only manageable via RPC commands currently.
   They only persist through the UTXO database and block database at this
   point.
   There are existing RPC commands to list and filter for tokens in the UTXO set.
   RPC raw transaction handling commands have been extended to allow creation
   (and sending) of token transactions.
   Interested users are advised to consult the functional test in
   `test/functional/bchn-rpc-tokens.py` for examples on token transaction
   construction and listing.
   Future releases will aim to extend the RPC API with more convenient
   ways to create and spend tokens, as well as upgrading the wallet storage
   and indexing subsystems to persistently store data about tokens of interest
   to the user. Later we expect to add GUI wallet management of Cash Tokens.

2. Transactions with SIGHASH_UTXO are not covered by DSProofs at present.

3. P2SH-32 is not used by default in the wallet (regular P2SH-20 remains
   the default wherever P2SH is treated).

4. The markup of Double Spend Proof events in the wallet does not survive
   a restart of the wallet, as the information is not persisted to the
   wallet.

5. The ABLA algorithm for BCH is currently temporarily set to cap the max block
   size at 2GB. This is due to limitations in the p2p protocol (as well as the
   block data file format in BCHN).


## Known Issues

Some issues could not be closed in time for release, but we are tracking all
of them on our GitLab repository.

- The minimum macOS version is 10.14 (Mojave).
  Earlier macOS versions are no longer supported.

- Windows users are recommended not to run multiple instances of bitcoin-qt
  or bitcoind on the same machine if the wallet feature is enabled.
  There is risk of data corruption if instances are configured to use the same
  wallet folder.

- Some users have encountered unit tests failures when running in WSL
  environments (e.g. WSL/Ubuntu).  At this time, WSL is not considered a
  supported environment for the software. This may change in future.
  It has been reported that using WSL2 improves the issue.

- `doc/dependencies.md` needs revision (Issue #65).

- For users running from sources built with BerkeleyDB releases newer than
  the 5.3 which is used in this release, please take into consideration
  the database format compatibility issues described in Issue #34.
  When building from source it is recommended to use BerkeleyDB 5.3 as this
  avoids wallet database incompatibility issues with the official release.

- The `test_bitcoin-qt` test executable fails on Linux Mint 20
  (see Issue #144). This does not otherwise appear to impact the functioning
  of the BCHN software on that platform.

- With a certain combination of build flags that included disabling
  the QR code library, a build failure was observed where an erroneous
  linking against the QR code library (not present) was attempted (Issue #138).

- Possible out-of-memory error when starting bitcoind with high excessiveblocksize
  value (Issue #156)

- A problem was observed on scalenet where nodes would sometimes hang for
  around 10 minutes, accepting RPC connections but not responding to them
  (see #210).

- Startup and shutdown time of nodes on scalenet can be long (see Issue #313).

- Race condition in one of the `p2p_invalid_messages.py` tests (see Issue #409).

- Occasional failure in bchn-txbroadcastinterval.py (see Issue #403).

- wallet_keypool.py test failure when run as part of suite on certain many-core
  platforms (see Issue #380).

- Spurious 'insufficient funds' failure during p2p_stresstest.py benchmark
  (see Issue #377).

- If compiling from source, secp256k1 now no longer works with latest openssl3.x series.
  There are workarounds (see Issue #364).

- Spurious `AssertionError: Mempool sync timed out` in several tests
  (see Issue #357).

- For some platforms, there may be a need to install additional libraries
  in order to build from source (see Issue #431 and discussion in MR 1523).

- More TorV3 static seeds may be needed to get `-onlynet=onion` working
  (see Issue #429).

- Memory usage can be very high if repeatedly doing RPC `getblock` with
  verbose=2 on a hash of known big blocks (see Issue #466).

- A GUI crash failure was observed when attempting to encrypt a large imported
  wallet (see Issue #490).

- The 'wallet_multiwallet' functional test fails on latest Arch Linux due to
  a change in semantics in a dependency (see Issue #505). This is not
  expected to impact functionality otherwise, only a particular edge case
  of the test.

- The 'p2p_extversion' functional test is sensitive to timing issues when
  run at high load (see Issue #501).

---

## Changes since Bitcoin Cash Node 27.1.0

### New documents

None

### Removed documents

None

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

- 8ec45680a2f7e992ec7b04ca8c7f1b7224f7bf9c [net] Ignore unlikely timestamps in version messages
- 99146608de09e5208ca1811a3b06bd1fd141b288 p2p: Don't use timestamps from inbound peers
- b5b6c2752b9f46b1a54a806be9727ee6465f9ae1 Implement CHIP-2021-05-vm-limits: Targeted Virtual Machine Limits
- 697725bc338e9b9623d35f93beaa0dba66d52038 Implement CHIP-2024-07-BigInt: High-Precision Arithmetic for Bitcoin Cash (on top of VM Limits)

#### Interfaces / RPC

- a89d5fd2486f6822eb0d0219ddf81fbf7449cfaa RPC: Add input tokenData and scriptPubKey to getrawtransaction verbosity=2
- 71bf5fcf6bcd923b66b6830fc373fa84655647f6 net: Keep trying to use UPnP when -upnp=1
- c66609c7c760affb601fb9786ddd69e3ab1ebcbe net: Add libnatpmp support
- 82a5e526c9e9025029fe78a8e2479166c8993007 net: Add -natpmp command line option
- 6f2b9b7f412edb5116dd33e0a15c7cf496ffcc94 rpc: Add getindexinfo RPC

#### Features in internal development: support for UTXO commitments

None

### Data directory changes

None

#### Performance optimizations

- 9f2f5351cd6580f5c18c8f93e5097f93ebb06999 Minor optimization in OP_CHECKDATASIG* -- avoid a extra copy
- c967c2312ca5e7ffb8697a551480a52a984fcccc net_processing: Avoid reading the block for MSG_FILTERED_BLOCK if no filter
- 6772bc42527fcba9362cd3bcca267c92eb6a7e54 [backport] http: set TCP_NODELAY when creating HTTP server
- ef5347ee49321f782ad0e756888bc287f1d14ad7 Update static seeds in preparation for v28 release
- 567b0fc123ee0bd62882866238a3784b2044d654 Update checkpoints for v28.0.0 release
- faf18ddc0bdb9aeedfe97221743238f1507b0402 Update "assume valid" and "minimum chain work" for v28.0.0 release

#### GUI

- a6f94bd514525e90a9bc93d1b63d3adce2b4a779 gui: Apply port mapping changes on dialog exit
- d0c1dbcef64039bac571ffa54c0db695b8c01bac gui: Add NAT-PMP network option

#### Code quality

- 0228ea5d82f788df368d585e16b6be7b77abebc5 [backport] Introduce utility function SysErrorString
- 06aaa0529ec84a2790fbf79f676c4384a405a260 Replace all usages of thread-unsafe strerror() with SysErrorString()
- 861aeb647323dbb90a2d08d0f81e1cf18357b088 Bugfix: Fix potential UB in AddToCompactExtraTransactions in net_processing.cpp
- 0dc085db51f231cb865aa65740e797d6648a5428 Fix compiler error in blockstorage.cpp for older compilers such as on ubuntu bionic
- a2d105406af0a32de3cf49314d27e273e0761d4c fs: use the correct Boost versions for deprecated functionality check
- f8577fabc8b2f342a5f76aa3c4751f0e49e1120c backport: refactor: Cleanup thread ctor calls
- 667fc6ad73bc60d1035ad978bfbb631d13858add refactor: Move port mapping code to its own module
- ebacf9cf7ec23b3eaa1d49a6d5f48e69a19d17ca net: Add flags for port mapping protocols
- f167208ab31a7536867f8262c66372cfacd891e0 Ensure natpmp unmapping of ports follows RFC 6886 and sets eport to 0
- 6054f2c292e0ba71b0b720b0646744cf2b4d0338 backport: Fix compile warning, fix UB, and optimize CoinUndoSpend
- ba4282b77f38aefa07c1be3b77f1317bb06da312 Trivial: Fix some compiler warnings seen on GCC 14.2.1
- 5f0e05323c63bdd417651809839b490b55be5978 net: Future-proof and prevent UB in CConnMan::SocketSendData for >2GiB messages
- 21c5ae93e527a1ebfe14abd8133d775328c3de3f util: Disallow negative mocktime
- 8668afb8dc4755880372de41a99eb11d6cf292a0 net: Avoid UBSan warning in ProcessMessage(...)
- 5831ef1b359bb214e60314a1aa411d486ac10db4 Trivial: Fix compiler warning on GCC 14.2 in src/bench/chained_tx.cpp
- 5a8c2a18fb37c9971e37d5a0bda578e81e1b9c7d Prevent UB in index/base.* my removing pure virtual GetName() function
- 5a94f01e8d6febb7e11d89b746b8944b7f777dd3 fuzz: Fix linking of fuzz.cpp and G_TRANSLATION_FUN
- a858b5803eeae645fe697cbac9937f84930586ca Nit in blockencodings.h: get rid of C-style typedef enum declaration
- 5056ff998d374f706656e3368d887cd9e6fed925 refactor: Move class `BlockValidationOptions` to consensus/validation.h

#### Documentation updates

- 94bcb55715244a67eedc9b37a885e6420945e1d5 Update documentation and gitian-build.py to reflect new workflow.
- baa4707a1a614747e7c8efcbb49341d924ef7cd7 doc: Add release notes for getindexinfo RPC

#### Build / general

- 783fbe9e79b38048d2b4d4cb54f43d3e1593f25e backport: [GITIAN] Pull gitian sources in our repo
- fc47c4b717914e5e99b4b679b28a245803906237 build: Propagate well-known vars into depends
- 60c09844ca67f5d4539663974c69acfcd2371dcd build: use C++17 in depends
- 6a3c14f691de4dcd011bd5a81b190df567179289 depends: Split boost into build/host packages + bump + cleanup
- 25ef17b9d29e08bf40d0033e88f1baf132e3008a Fixup to toolchain files to get them to see the depends boost lib
- 761c2852db7a00db6ecd3df504d306388aebb9db [GITIAN] Don't ignore target-bin/
- 9ad50023b35d4cd9dc090205f96db76b5c13d9a4 Suppress linting for gitian-builder
- 9dddaf8254cade73bb66661aa872f6ef29378b2a [backport] cmake: define STRERROR_R_CHAR_P
- 4c4e89b76b34bd5d7cca133a6cffd6fcaf1ef8c9 backport: upgrade to miniupnpc 2.2.2
- b59af944d95102c465262e116594e502679a88a3 gitian: Disable NATPMP when building package sources
- 3b973eeb7b01b3cf2c39d4aedba4ff8021ab8c16 Add config to disable test_bitcoin & bench_bitcoin
- 9ecd9c4130ea946fcea4c6dbb8ef1bc4cb4161d7 Bump version to 28.0.0

#### Build / Linux

None

#### Build / Windows

None

#### Build / MacOSX

None

#### Tests / test framework

- 0cd881ac634e347e6ecdb4c842bf940e03031ff3 LibAuth tests: Rename/refactor to support dropping-in of non-CHIP tests & benchmarks
- 58c90ea80b6eb0f8362665cd297657831b5ec6b6 Imported latest LibAuth regression tests for the "2023" VM.
- 81cd2b124dd3ccb73784b60e19466f0290add7be Put test_bitcoin common files into a library, use with bench_bitcoin
- d2f65fe08d663455901d22d0b3cc1c0fef490368 tests: fix linting error in test_framework
- ecebf503380da90bad5fcd389ffcd2b03ef50cf3 tests: fix wallet_multiwallet invalid wallet path expected message
- a09d04e6883a40dc97a73ddd8e8925e2ce2b802f Update LibAuth test vector and benchmark scheme (on top of BigInt + VMLimits CHIP)
- 3fbe5385daf932a5d77632cbf835e3e94dee5fa4 rpc: Add getindexinfo RPC
- c9a7d1877fc09a4a86dea3df43fce7d8fd2cf86f tests: Add FuzzedDataProvider fuzzing helper from the Chromium project
- 790459cad8a68cac76e5b57042b2aca5a505b3e9 tests: Add fuzz utility header `fuzz/util.h`
- 492a6de30ef1532672fd76ec703cc2c92cb7d200 [block encodings] Make CheckBlock mockable for PartiallyDownloadedBlock
- cf5cdf1fb6410b6f93d4929536cca00058f3dc01 [block encodings] Avoid fuzz blocking asserts in PartiallyDownloadedBlock
- e357f5459b0762ec8bd750a1180915bcb8d84b69 Add fuzz/util function: ConsumeMempoolEntry
- 86237a07690c1503beb98ce3e123bde2a07b6db6 fuzz: Add PartiallyDownloadedBlock target

#### Benchmarks

- 1599f4b178a857f1afda629de0fef9c9b239f130 bench: nit: Don't double-copy in VerifyScript.* benchmarks
- 4c784dda34f99df168e765f933a577ed7ebb492f Added detection of "benchmark" and "baseline" LibAuth tests
- ea14cc66830de1fa42d610137441d6191850ef8a Enable running the LibAuth benchmarks in bench_bitcoin
- 0bd8a5802d17e674e961b701e8333dded3633058 bench: Support printing of supplemental stats; use this mechanism for LibAuth benches

#### Seeds / seeder software

- 6ebd56644903873ec03c54fd15f3c3a544a219ea seeder: Don't use std::rand(); it's not guaranteed thread-safe
- 08e4c3010c8b2b132008c2964823646dc794d7d6 seeder: Refactor C-style polymorphism -> C++ for CDnsThread
- 6693b7bfd4a5d64c37415b0bee50c6d8d37b458d seeder: Fix potential UB/race condition when starting DNS threads
- 9411c7a4161a74ffb7923c335986dbb17ac476e8 seeder: Fix bug where if DNS server can't start, app is silent with no indication of error
- e5c1c54dd54e3ebb08f0b6ba91ef5294687b6bd0 seeder: Refactor saving guts of ThreadDumper() to a separate function.
- 2dc062e522d4ebd758de199e327d2328a73dbfa6 seeder: Resurrect the (long deleted) "CAddrDb::Skipped" function(s)
- 402e22d17dab26074e56524bf67c39e57c3036f1 seeder: Fix potential C++ UB by avoiding passing pointers through void *
- 7653e1834914ac59ef70fd9d3a06546fb50b0e77 seeder: Refactor to use std::thread instead of pthread where appropriate
- 9e511fef309e3f5bda1d5e968a50af5acb7b777a seeder: Gracefully exit and always persist db immediately on exit
- f1ce09f06394da028a6a1b1e3042a2cdd0aebc05 Fixed a rare crash bug when process runs out of file descriptors
- a3a9c127adaf4a1928bbcb4f597f04356024883c Use SysErrorString instead of strerror

#### Maintainer tools

- 7be3d54a2d7733bcbac88501302a4427d3da0454 lint: Add check for usage of strerror in the codebase
- 2e02b9067a616a0c74400fb6f18ddf0e36fe80d3 Add release notes 27.1.0 to docs site navigation
- 57d383e2c2d73731c84723f78cab789a81ffcf70 linter: Check for C header includes
- 4ff1b5d041a8b0fb4ad022900d07c2af139cb35d linter: Check for C-style void parameter
- ce905c2f2fe63c897a08473d8ecea6f9eac98871 linter: Source files must match naming convention
- 1403bc580841bb54213cf3edc22cced9ab334ac9 linter: avoid locale dependencies
- 4a9a82e583c680216e2f387507c2d4418e0da912 Nuke lint-local-dependencies
- f181772861bbf863b805ebe6913c942b0f05e18c Nuke lint-cpp-void-parameters

#### Infrastructure

None

#### Cleanup

- 5cd212189f4591b6de1d1d513f53e4b22808566f scripted-diff: Rename UPnP stuff
- b007a9c4c21323e65855a037d91e38f2740eb38b net: Add NAT-PMP to port mapping loop
- edc974e8619ed54d1a91b01a88b94e517c2aa80c Bump node expiry to May 2026, add new `-upgrade12activationtime`.
- 48fbce146f85cc276783c96b7a51c4c729f5b9a4 Upgrade12: Update activation_tests, test_bitcoin, and the g_upgrade*tracker
- 0c2bc7698feb507362ca84000fc6c2053de93ff1 Fix ninja target: check-upgrade-activated
- 336132a99f19690a18f5e562452a13432f6d7150 net: remove is{Empty,Full} flags from CBloomFilter, clarify CVE fix

#### Continuous Integration (GitLab CI)

- 1975b094b3f6e94577cf1ba812a1aebd0ac3fe74 CI: update to use libnatpmp

#### Compatibility

- 3eb874e2886540ced618ceb49fd1b8341e235e20 [backport] upnp: add compatibility for miniupnpc 2.2.8
- 4f4fc44421548911517561ecab97725a578ff533 depends: Fix boost compile error for aarch64 & arm with newer gcc
- dd853135503057209aee8993d613bf80032aefed Drop support for 32-bit platforms

#### Backports

- 783fbe9e79b38048d2b4d4cb54f43d3e1593f25e backport: [GITIAN] Pull gitian sources in our repo
- fc47c4b717914e5e99b4b679b28a245803906237 build: Propagate well-known vars into depends
- 60c09844ca67f5d4539663974c69acfcd2371dcd build: use C++17 in depends
- 6a3c14f691de4dcd011bd5a81b190df567179289 depends: Split boost into build/host packages + bump + cleanup
- 761c2852db7a00db6ecd3df504d306388aebb9db [GITIAN] Don't ignore target-bin/
- 9dddaf8254cade73bb66661aa872f6ef29378b2a [backport] cmake: define STRERROR_R_CHAR_P
- 0228ea5d82f788df368d585e16b6be7b77abebc5 [backport] Introduce utility function SysErrorString
- 06aaa0529ec84a2790fbf79f676c4384a405a260 Replace all usages of thread-unsafe strerror() with SysErrorString()
- 3eb874e2886540ced618ceb49fd1b8341e235e20 [backport] upnp: add compatibility for miniupnpc 2.2.8
- 4c4e89b76b34bd5d7cca133a6cffd6fcaf1ef8c9 backport: upgrade to miniupnpc 2.2.2
- f8577fabc8b2f342a5f76aa3c4751f0e49e1120c backport: refactor: Cleanup thread ctor calls
- 667fc6ad73bc60d1035ad978bfbb631d13858add refactor: Move port mapping code to its own module
- 71bf5fcf6bcd923b66b6830fc373fa84655647f6 net: Keep trying to use UPnP when -upnp=1
- ebacf9cf7ec23b3eaa1d49a6d5f48e69a19d17ca net: Add flags for port mapping protocols
- 5cd212189f4591b6de1d1d513f53e4b22808566f scripted-diff: Rename UPnP stuff
- a6f94bd514525e90a9bc93d1b63d3adce2b4a779 gui: Apply port mapping changes on dialog exit
- c66609c7c760affb601fb9786ddd69e3ab1ebcbe net: Add libnatpmp support
- b007a9c4c21323e65855a037d91e38f2740eb38b net: Add NAT-PMP to port mapping loop
- 82a5e526c9e9025029fe78a8e2479166c8993007 net: Add -natpmp command line option
- d0c1dbcef64039bac571ffa54c0db695b8c01bac gui: Add NAT-PMP network option
- 6054f2c292e0ba71b0b720b0646744cf2b4d0338 backport: Fix compile warning, fix UB, and optimize CoinUndoSpend
- 8ec45680a2f7e992ec7b04ca8c7f1b7224f7bf9c [net] Ignore unlikely timestamps in version messages
- 99146608de09e5208ca1811a3b06bd1fd141b288 p2p: Don't use timestamps from inbound peers
- 21c5ae93e527a1ebfe14abd8133d775328c3de3f util: Disallow negative mocktime
- 8668afb8dc4755880372de41a99eb11d6cf292a0 net: Avoid UBSan warning in ProcessMessage(...)
- 6772bc42527fcba9362cd3bcca267c92eb6a7e54 [backport] http: set TCP_NODELAY when creating HTTP server
- 6f2b9b7f412edb5116dd33e0a15c7cf496ffcc94 rpc: Add getindexinfo RPC
- 3fbe5385daf932a5d77632cbf835e3e94dee5fa4 rpc: Add getindexinfo RPC
- baa4707a1a614747e7c8efcbb49341d924ef7cd7 doc: Add release notes for getindexinfo RPC
- 5a8c2a18fb37c9971e37d5a0bda578e81e1b9c7d Prevent UB in index/base.* my removing pure virtual GetName() function
- 336132a99f19690a18f5e562452a13432f6d7150 net: remove is{Empty,Full} flags from CBloomFilter, clarify CVE fix
- c9a7d1877fc09a4a86dea3df43fce7d8fd2cf86f tests: Add FuzzedDataProvider fuzzing helper from the Chromium project
- 790459cad8a68cac76e5b57042b2aca5a505b3e9 tests: Add fuzz utility header `fuzz/util.h`
- 492a6de30ef1532672fd76ec703cc2c92cb7d200 [block encodings] Make CheckBlock mockable for PartiallyDownloadedBlock
- cf5cdf1fb6410b6f93d4929536cca00058f3dc01 [block encodings] Avoid fuzz blocking asserts in PartiallyDownloadedBlock
- e357f5459b0762ec8bd750a1180915bcb8d84b69 Add fuzz/util function: ConsumeMempoolEntry
- 86237a07690c1503beb98ce3e123bde2a07b6db6 fuzz: Add PartiallyDownloadedBlock target
