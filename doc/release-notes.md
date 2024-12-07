# Release Notes for Bitcoin Cash Node version 27.1.1

Bitcoin Cash Node version 27.1.1 is now available from:

  <https://bitcoincashnode.org>

## Overview

This release of Bitcoin Cash Node (BCHN) includes ... TODO

## Usage recommendations

TODO

## Network changes

TODO

## Added functionality

- Added NAT-PMP port mapping support via [`libnatpmp`](https://miniupnp.tuxfamily.org/libnatpmp.html).
  Use the `-natpmp` command line option to use NAT-PMP to map the listening port. If both UPnP
  and NAT-PMP are enabled, a successful allocation from UPnP prevails over one from NAT-PMP.

## Deprecated functionality

TODO

## Modified functionality

TODO

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

TODO

## Regressions

Bitcoin Cash Node 27.1.1 does not introduce any known regressions as compared to 27.1.0.

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

- Nodes on 32-bit platforms could fail if blocks reach a size over 1GiB
  (per ABLA this would take multiple years, and by then 32-bit platforms
  will no longer be supported in any case).
  Technically, this is because blocks may take 2x-3x as much memory when
  unserialized into memory as they do when serialized, and 32-bit machines
  often cannot address more than 2GiB in a userspace process, and can never
  address more than 4GiB.

- The 'wallet_multiwallet' functional test fails on latest Arch Linux due to
  a change in semantics in a dependency (see Issue #505). This is not
  expected to impact functionality otherwise, only a particular edge case
  of the test.

- The 'p2p_extversion' functional test is sensitive to timing issues when
  run at high load (see Issue #501).

---

## Changes since Bitcoin Cash Node 27.0.0

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
