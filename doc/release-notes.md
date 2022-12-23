# Release Notes for Bitcoin Cash Node version 25.0.0

Bitcoin Cash Node version 25.0.0 is now available from:

  <https://bitcoincashnode.org>

## Overview

This is a minor release of Bitcoin Cash Node (BCHN).

This version contains further corrections and improvements, such as:

- TODO

Users who are running any of our previous releases (24.x.y or 25.0.0)
are encouraged to upgrade to v25.0.1.

## Usage recommendations

TODO

## Network changes

TODO

## Added functionality

TODO

## Deprecated functionality

TODO

## Modified functionality

- The `getblock` RPC command has been modified: verbosity level 2 now returns `fee`
  information per transaction in the block. Additionally a new verbosity level 3 has
  been added which is like level 2 but each transaction will include inputs' `prevout`
  information.  The existing `/rest/block/` REST endpoint is modified to contain
  this information too. Every `vin` field will contain an additional `prevout` subfield
  describing the spent output. `prevout` contains the following keys:
  - `generated` - true if the spent coin was a coinbase
  - `height`
  - `value`
  - `scriptPubKey`
  - `tokenData` (after May 2023 upgrade, appears if the transaction input had token data)


## Removed functionality

TODO

## New RPC methods

TODO

## User interface changes

TODO

## Regressions

Bitcoin Cash Node 25.0.1 does not introduce any known regressions as compared to 25.0.0.

## Limitations

TODO

## Known Issues

Some issues could not be closed in time for release, but we are tracking all of them on our GitLab repository.

- The minimum macOS version is 10.14 (Mojave). Earlier macOS versions are no longer supported.

- Windows users are recommended not to run multiple instances of bitcoin-qt
  or bitcoind on the same machine if the wallet feature is enabled.
  There is risk of data corruption if instances are configured to use the same
  wallet folder.

- Some users have encountered unit tests failures when running in WSL
  environments (e.g. WSL/Ubuntu).  At this time, WSL is not considered a
  supported environment for the software. This may change in future.

  The functional failure on WSL is tracked in Issue #33.
  It arises when competing node program instances are not prevented from
  opening the same wallet folder. Running multiple program instances with
  the same configured walletdir could potentially lead to data corruption.
  The failure has not been observed on other operating systems so far.

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

- At the time of writing, scalenet has not been mined for a while (since
  10 August 2022), thus synchronization will stop at that block time.
  This isn't considered a BCHN issue per se, it depends on miners to
  resume mining this testnet.

- Startup and shutdown time of nodes on scalenet can be long (see Issue #313).

- Race condition in one of the `p2p_invalid_messages.py` tests (see Issue #409).

- Occasional failure in bchn-txbroadcastinterval.py (see Issue #403).

- wallet_keypool.py test failure when run as part of suite on certain many-core
  platforms (see Issue #380).

- Spurious 'insufficient funds' failure during p2p_stresstest.py benchmark
  (see Issue #377).

- If compiling from source, secp256k1 now no longer works with latest openssl3.x series. There are
  workarounds (see Issue #364).

- Spurious `AssertionError: Mempool sync timed out` in several tests
  (see Issue #357).

- For some platforms, there may be a need to install additional libraries
  in order to build from source (see Issue #431 and discussion in MR 1523).

- More TorV3 static seeds may be needed to get `-onlynet=onion` working
  (see Issue #429).

---

## Changes since Bitcoin Cash Node 25.0.0

### New documents

TODO

### Removed documents

TODO

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

None.

#### Interfaces / RPC

None.

### Data directory changes

None.

#### Performance optimizations

None.

#### GUI

None.

#### Code quality

None.

#### Documentation updates

None.

#### Build / general

None.

#### Build / Linux

None.

#### Build / Windows

None.

#### Build / MacOSX

None.

#### Tests / test framework

None.

#### Benchmarks

None.

#### Seeds / seeder software

None.

#### Maintainer tools

None.

#### Infrastructure

None.

#### Cleanup

None.

#### Continuous Integration (GitLab CI)

None.

#### Backports

None.
