# Release Notes for Bitcoin Cash Node version 28.0.1

Bitcoin Cash Node version 28.0.1 is now available from:

  <https://bitcoincashnode.org>

## Overview

This release of Bitcoin Cash Node (BCHN) is a patch release. It includes various corrections and improvements, most
notably a performance improvement for large block processing, which should benefit non-mining and mining nodes alike.

## Usage recommendations

Users who are running v28.0.0 or older are encouraged to upgrade to v28.0.1.

## Network changes

None

## Added functionality

None


## Deprecated functionality

None

## Modified functionality

- The peer-to-peer network's block propagation logic has been improved to allow for lower-latency block propagation.
  In particular, block downloads now request the latest block from up to 3 peers simultaneously so that the node has a
  better chance of receiving the latest block as quickly as possible.
- The ABLA startup checks have been simplified and reduced to be simpler and faster. This should improve Bitcoin Cash
  Node startup times (in particular if running on an HDD-based system). To enable the old more thorough ABLA checks at
  app startup, start the node with the `-check-abla` option.

## Removed functionality

None

## New RPC methods

None

## User interface changes

- The `getpeerinfo` RPC returns two new boolean fields, `bip152_hb_to` and
  `bip152_hb_from`, that respectively indicate whether we selected a peer to be
  in compact blocks high-bandwidth mode or whether a peer selected us as a
  compact blocks high-bandwidth peer. High-bandwidth peers send new block
  announcements via a `cmpctblock` message rather than the usual inv/headers
  announcements. See BIP 152 for more details.
- The `getnetworkinfo` RPC method results now include two new keys: `connections_in` and `connections_out`. These
  correspond to the current number of active inbound and outbound peer-to-peer connections, respectively.

## Regressions

Bitcoin Cash Node 28.0.1 does not introduce any known regressions as compared to 28.0.0.

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

## Changes since Bitcoin Cash Node 28.0.0

### New documents

None

### Removed documents

None

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

- 5ac80029444ad7c065206903064c35f88d056a22 net: Fix potential for UB in `CNode::ReceiveMsgBytes`
- 2b92f731ff52d61379047ec21868adc69ea15f42 mempool: Fix potential for bug in mempool removeForBlock()

#### Interfaces / RPC

- 2aaadb60b8e31694be50b273cfc3c680dace5096 net, rpc: expose high bandwidth mode state via getpeerinfo

#### Features in internal development: support for UTXO commitments

None

### Data directory changes

None

#### Performance optimizations

- 6961e643de353b94823e834e1c7046ffe888150a net: Avoid excess CPU usage by `msghand` thread; fix "early wake up" logic
- fa0d374801ad0001a132569bed047fdb95e19516 Improve `DisconnectedBlockTransactions::addForBlock` performance
- ecd1392cfeeaffc87d66179e91e902daae0abffa ABLA: Improve performance of app init, turn off slower checks by default
- ac22ffd9b05ef592cf044f8decc91cafd1191de9 [net] Parallel compact block downloads

#### GUI

None

#### Code quality

- 70b94b05b50dc40146587ae257d0bab3e84672d1 [net processing] Tidy up sendcmpct processing

#### Documentation updates

- 4a8036c008c677ef4054b0b257c6a2056918eb7b Update Ubuntu and Debian build guide

#### Build / general

None

#### Build / Linux

None

#### Build / Windows

None

#### Build / MacOSX

None

#### Tests / test framework

- ae96a7f02927ae0954ce5456872f63362cfbf323 Improvements to test bchn-txbroadcastinterval.py
- 3e6cff52473acd41ea2213dc8d627375646ed6a0 [tests] Allow outbound connections in functional tests.
- c2f4061ec7d40d2f9ab4e62dd7b56b067d090581 util: Add CHECK_NONFATAL and use it in src/rpc
- 1d5a02e8c8de06fb57243a4260668c3ba8b890b6 Added partial backport of core header/cpp file "util/check.*"
- 124e41bb791a6b95db810ca39ea15d386a1eacd6 Add tests for parallel compact block downloads

#### Benchmarks

None

#### Seeds / seeder software

None

#### Maintainer tools

None

#### Infrastructure

None

#### Cleanup

- b18fb57c2eb72e628ff8270964c4b53c927efc5a [qa] Bump version to 28.0.1, rotate release notes
- a69c5c6c667c2d1b006422ac57a0fd84f990a4e6 net: Remove extraneous/bad log line from net_processing.cpp
- e887c38afe32abff1a8befececf41d4f16f22524 Update DisconnectedBlockTransactions::addForBlock to use CHECK_NONFATAL rather than assert()

#### Continuous Integration (GitLab CI)

None

#### Compatibility

None

#### Backports

- 3e6cff52473acd41ea2213dc8d627375646ed6a0 [tests] Allow outbound connections in functional tests.
- c2f4061ec7d40d2f9ab4e62dd7b56b067d090581 util: Add CHECK_NONFATAL and use it in src/rpc
- 1d5a02e8c8de06fb57243a4260668c3ba8b890b6 Added partial backport of core header/cpp file "util/check.*"
- 2aaadb60b8e31694be50b273cfc3c680dace5096 net, rpc: expose high bandwidth mode state via getpeerinfo
- 70b94b05b50dc40146587ae257d0bab3e84672d1 [net processing] Tidy up sendcmpct processing
- ac22ffd9b05ef592cf044f8decc91cafd1191de9 [net] Parallel compact block downloads
- 124e41bb791a6b95db810ca39ea15d386a1eacd6 Add tests for parallel compact block downloads
