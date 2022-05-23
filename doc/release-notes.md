# Release Notes for Bitcoin Cash Node version 24.1.0

Bitcoin Cash Node version 24.1.0 is now available from:

  <https://bitcoincashnode.org>

## Overview

This is a minor release of Bitcoin Cash Node (BCHN) that implements ...

This version contains further corrections and improvements, such as:

...

## Usage recommendations

...

## Network changes

- The Tor onion service that is automatically created by setting the
  `-listenonion` configuration parameter will now be created as a Tor v3 service
  instead of Tor v2. The private key that was used for Tor v2 (if any) will be
  left untouched in the `onion_private_key` file in the data directory (see
  `-datadir`) and can be removed if not needed. Bitcoin Core will no longer
  attempt to read it. The private key for the Tor v3 service will be saved in a
  file named `onion_v3_private_key`. To use the deprecated Tor v2 service (not
  recommended), then `onion_private_key` can be copied over
  `onion_v3_private_key`, e.g.
  `cp -f onion_private_key onion_v3_private_key`.

- If using the `-listenonion=1` option (default: 1 if unspecified), then an
  additional new bind `address:port` will be used for hidden-service-only incoming
  .onion connections. On mainnet, this will be 127.0.0.1:8334. This bind address
  is in **addition** to any `-bind=` options specified on the CLI and/or in the
  config file.
  - To specify the local onion port to bind to explicitly, use the new syntax,
    `-bind=<HOST>:<PORT>=onion`. See `bitcoind -help` for more information on
    the new `=onion` syntax which is used to specify local tor listening ports.
  - To not create an additional bind endpoint for tor, and/or to disable tor
    hidden service support, use `-listenonion=0`.

  Note: As before, tor hidden services are always advertised publicly using a
  "generic" port for maximal anonymity (such as 8333 on mainnet, 18333 on
  testnet3, and so on). The `-bind=<HOST>:<PORT>=onion` syntax is to simply
  specify the local bind address which is connected-to privately by the
  local `tor` process when a remote node connects to your .onion hidden
  service.

## Added functionality

- Added a new logging option, `-debug=httptrace` which logs all HTTP data to/from the internal JSON-RPC and REST server, including HTTP content.
    - This is an advanced debugging option intended for troubleshooting the low-level JSON-RPC and/or REST protocol, primarily aimed at developers.
    - This option is not enabled by default, even if using `-debug=all`, and must be explicitly enabled using `-debug=httptrace`.
    - Unlike the regular `-debug=http` debug option, incoming strings and data are *not* sanitized in any way in the log file, and are simply logged verbatim as they come in off the network.

## Deprecated functionality

...

## Modified functionality

- The `-whitelistrelay` default has been restored to enabled. The help text indicated the default being enabled, despite the default actually being disabled. This has been broken since v22.0.0.

## Removed functionality

...

## New RPC methods

...

## User interface changes

...

## Regressions

Bitcoin Cash Node 24.0.1 does not introduce any known regressions as compared to 24.0.0.

## Known Issues

Some issues could not be closed in time for release, but we are tracking all of them on our GitLab repository.

- MacOS versions earlier than 10.12 are no longer supported. Additionally,
  Bitcoin Cash Node does not yet change appearance when macOS "dark mode"
  is activated.

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

- Some functional tests are known to fail spuriously with varying probability.
  (see e.g. issue #148, and a fuller listing in #162).

- Possible out-of-memory error when starting bitcoind with high excessiveblocksize
  value (Issue #156)

- A problem was observed on scalenet where nodes would sometimes hang for
  around 10 minutes, accepting RPC connections but not responding to them
  (see #210).

- Startup and shutdown time of nodes on scalenet can be long (see Issue #313).

- On some platforms, the splash screen can be maximized, but it cannot be
  unmaximized again (see #255). This has only been observed on Mac OSX,
  not on Linux or Windows builds.

- There is an issue with `git-lfs` that may interfere with the refreshing
  of source code checkouts which have not been updated for a longer time
  (see Issues #326, #333). A known workaround is to do a fresh clone of the
  repository.

---

## Changes since Bitcoin Cash Node 24.0.0

### New documents

...

### Removed documents

...

### Notable commits grouped by functionality

...

#### Security or consensus relevant fixes

...

#### Interfaces / RPC

- Netmasks that contain 1-bits after 0-bits (the 1-bits are not contiguous on
  the left side, e.g. 255.0.255.255) are no longer accepted. They are invalid
  according to RFC 4632.

#### Performance optimizations

...

#### GUI

...

#### Data directory changes

- The node's known peers are persisted to disk in a file called `peers.dat`.
  The format of this file has been changed in a backwards-incompatible way in
  order to accommodate the storage of Tor v3 and other BIP155 addresses. This
  means that if the file is modified by this version or newer then older
  versions will not be able to read it. Those old versions, in the event of a
  downgrade, will log an error message that deserialization has failed and will
  continue normal operation as if the file was missing, creating a new empty
  one.

#### Code quality

...

#### Documentation updates

...

#### Build / general

...

#### Build / Linux

...

#### Build / Windows

...

#### Build / MacOSX

...

#### Tests / test framework

...

#### Benchmarks

...

#### Seeds / seeder software

...

#### Maintainer tools

...

#### Infrastructure

...

#### Cleanup

...

#### Continuous Integration (GitLab CI)

...

#### Backports

...
