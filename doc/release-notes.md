# Release Notes for Bitcoin Cash Node version 26.0.1

Bitcoin Cash Node version 26.0.1 is now available from:

  <https://bitcoincashnode.org>

## Overview

This release of Bitcoin Cash Node (BCHN) is a minor release.

It includes the following:

...

## Usage recommendations

TODO

## Network changes

Unsolicited `ADDR` messages from network peers are now rate-limited to an average rate of 0.1 addresses per second,
with allowance for burts of up to 1000 addresses. This change should have no noticeable deleterious effect on the
peer-to-peer network for well-behaved peers, and was merely added as a performance optimization.

## Added functionality

TODO

## Deprecated functionality

TODO

## Modified functionality

- The `getpeerinfo` RPC method results now include two new additional data items for each peer: `addr_processed` and
  `addr_rate_limited`. See the `getpeerinfo` RPC help for a description of these new items.

## Removed functionality

TODO

## New RPC methods

TODO

## User interface changes

TODO

## Regressions

Bitcoin Cash Node 26.0.1 does not introduce any known regressions as compared to 26.0.0.

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

- Memory usage can be very high if repeatedly doing RPC `getblock` with
  verbose=2 on a hash of known big blocks (see Issue #466).

---

## Changes since Bitcoin Cash Node 25.0.0

### New documents

TODO

### Removed documents

TODO

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

TODO

#### Interfaces / RPC

TODO

#### Features in internal development: support for UTXO commitments

TODO

### Data directory changes

TODO

#### Performance optimizations

TODO

#### GUI

TODO

#### Code quality

TODO

#### Documentation updates

TODO

#### Build / general

TODO

#### Build / Linux

TODO

#### Build / Windows

TODO

#### Build / MacOSX

TODO

#### Tests / test framework

TODO

#### Benchmarks

TODO

#### Seeds / seeder software

TODO

#### Maintainer tools

TODO

#### Infrastructure

TODO

#### Cleanup

TODO

#### Continuous Integration (GitLab CI)

TODO

#### Backports

TODO
