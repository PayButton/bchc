# Release Notes for Bitcoin Cash Node version 27.1.0

Bitcoin Cash Node version 27.1.0 is now available from:

  <https://bitcoincashnode.org>

## Overview

This release of Bitcoin Cash Node (BCHN) includes some bug fixes and important reliability enhancements.

## Usage recommendations

Users who are running v27.0.0 or earlier are encouraged to upgrade to v27.1.0 as it includes some bugfixes as well as
updated checkpoints for the recently completed (15 May 2024) network upgrade. It also include some performance and
reliability fixes related to transaction processing.

## Network changes

The internal transaction processing logic for both regular and orphan transactions has been reworked to be more
performant and more reliable.

## Added functionality

None.

## Deprecated functionality

None.

## Modified functionality

The command-line option -upgrade10activationtime has been renamed to -upgrade10activationheight and is now height-based,
rather than MTP time-based. The next upgrade is still expected to be MTP time-based.

## Removed functionality

BIP70 payment request support has been removed from the Qt-based GUI. The
reason for this is that BIP70 payment requests have historically suffered
from a number of security flaws and maintainence of this feature poses a
risk to full node software. Users wishing to use a GUI wallet that supports
BIP70  are recommended to use one of the many light wallets such as Electron
Cash or Bitcoin.com that continue to support BIP70 payment requests.

## New RPC methods

None.

## User interface changes

- The `-wipeignore` bitcoin-seeder command line option has been removed. The way the
  list of ignored nodes is handled has been broken for many years. Rather than ignoring
  nodes *until* a particular time, nodes were being ignored *from* that time. This meant
  that ignored nodes that ever reached this timeout would stay ignored forever; and
  until that point, they would be processed as often as any other node rather than
  being ignored for a cooling-off period as intended. To resolve this, we decided to
  simply remove this facility entirely, meaning that nodes will now never be ignored.
- The `datadir=` conf file variable appearing both outside and inside a section header in
  the conf file works as expected now, and no longer leads to unexpected `blocksdir=`
  settings (!1816).

## Regressions

Bitcoin Cash Node 27.1.0 does not introduce any known regressions as compared to 27.0.0.

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

None

### Removed documents

None

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

- 2443f184a582b2bf726f4fdce131c4cbd4c08324 Add checkpoints for blocks after upgrade10 ABLA activation
- 7f862714801cf2ae764e4bc0d74831c8f2ef8920 Switch ABLA (upgrade10) to use height-based activation, rather than MTP
- 399f414b4be9539b5b41b4c82d02ba45ff5b3c42 qa: Update chainparams assumevalid and minimumchainwork for 27.1.0
- 02c658c8e3d3d2ab3d7c7e2cdbc3137134d2def9 Update chainTxData for main, test3, test4, and chipnet

#### Interfaces / RPC

None

#### Features in internal development: support for UTXO commitments

None

### Data directory changes

- c64905bdd52c227f1901a3d183e20ee7ace6fbf6 Fix for wrong behavior for conf file datadir= inside vs outside a secton

#### Performance optimizations

None

#### GUI

None

#### Code quality

- 4f4fa009aa944d156d81d43dc1e69649eb8d2153 Trivial: Fix compiler warning about unused variable in dsproof_tests.cpp
- 706bc01a8a298dcf296a379b2b853d0090cdd8d5 Remove deprecated boost::basename method.
- ef2dcc2176a12ddac4f54b73bd372a9748a2c88f Fix compatibility for Boost 1.85 and above
- 368c9996c12b36fd700f1ce7c0933d0b020412ba trivial: Fix up the header util/heapoptional.h includes
- 0f6a086ce91ff3aa7b2faaa748efa48a3e611d9a Fix spurious ABLA-related warning for pruned nodes

#### Documentation updates

- 3c137713ddef3e2cced98866da97c28a0fde5e20 Update copyright year to 2024
- 633908d627f65a4f997b17121efea17856295d44 clarify links and link contribution guide from docs
- 6fbefbf3b6c1cb80653118f3b21f7da3d8c9d685 Include onlynet option in example bitcoin.conf
- 9ffaa384771b1df2c9d88b3829db62957e2b3fba Bump version to 27.1.0 + update release-notes to use this version

#### Build / general

None

#### Build / Linux

None

#### Build / Windows

None

#### Build / MacOSX

None

#### Tests / test framework

- dba4975cb98f9453cb5084f18e02fe316a0c355a Nit: Fix a missing `%s` in chip_testing_setup.cpp
- 233c8cfa7e400b2aa8f4e9b572a8ef0816d617c5 Expected test failure messages regeneration
- 2ff9aeaa7ac80c72580b3f6215b641351bfb6752 Expand signing serialization VMB tests


#### Benchmarks

None

#### Seeds / seeder software

- 93e08e6642dbd533cccaac03b79df183dfaba366 Fix seeder checkpoint check
- 9cdabd25c642c79bb1857b6fe64696b846e873e4 Fixed a rare crash bug when process runs out of file descriptors
- 7ddc65cc572b48be5c31041790190aa71e22bcd7 Fix socket leaks leading to process running out of file descriptors


#### Maintainer tools

- 6b79e9fef20cb337c17f73bc786ffa7f17120163 linearize: Add the new testnets and rename diskmagic

#### Infrastructure

None

#### Cleanup

- 37ac1c6058c0aa639297e8166108d15ddc54429b Deleted comments related to segwit.

#### Continuous Integration (GitLab CI)

- 610dfff8da4f2ca643bcd6c04c384b07b53254ca Import CI docker image file
- b7b2e762105b70f7d739da730e70d4fc73378468 ci: Bump docker image buster -> bookworm


#### Backports

- 975743a8f0912bb580d30989bbe65a7727da85d8 Add the the "util::Overloaded" helper for more idiomatic std::variant usage
- 29e9616a1ff7f40dbdcb236d0cb09cfbeb02fdfd net processing: Only send a getheaders for one block in an INV
- 2b563318ce6e9a681710c873d3ef54bfd5a5d9b2 Avoid the use of abs64 in timedata
- e5c1897d2b4557aad6088ece40bc741dd55c1410 Simplify orphan processing in preparation for interruptibility
- 1d91bcc2385272c1e14c16d2cc33e97ddf7a82d6 MOVEONLY: Move processing of orphan queue to ProcessOrphanTx
- 218969139a6ab51bfcc2351003478becd09013bf Interrupt orphan processing after every transaction
- 25fe0ea589fa0ee751f6bef89810bf6b42f2531e Removed BIP70 support
- d5e0e02bf00bd4ac796be72d35ea19b72665d4fa Add txrequest module
- dc9959798bba2a877874c9647ab635c0a7f7460f Add txrequest unit tests
- a44ac5a96514bfb654a75bb4fb7854732d73feea Add txrequest fuzz tests
- 5b85c5dcc395992c7c90250cb71ed957316d2389 Change transaction request logic to use txrequest
- 7675a909b6eed0f49cb7ba460018d1b726e868a4 Expedite removal of tx requests that are no longer needed
- d9e0b6f0efb50605c9666fd4d50bb531defe69a8 Delete limitedmap as it is unused now
- d4075a4bffc1d7e4deea4ccca634a3e2b65b9d8c Report and verify expirations
- 63069ce9d074461ee8aa97f6873be679b7265eba Add "overload' functional test + disable the penalty for PF_RELAY peers
- d01f330e6c6046eba24008897655f9c713d4a5c9 p2p: declare Announcement::m_state as uint8_t, add getter/setter
