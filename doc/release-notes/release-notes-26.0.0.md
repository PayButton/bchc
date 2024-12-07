# Release Notes for Bitcoin Cash Node version 26.0.0

Bitcoin Cash Node version 26.0.0 is now available from:

  <https://bitcoincashnode.org>

## Overview

This release of Bitcoin Cash Node (BCHN) is marked as a major release but
not due to consensus changes - only because it changes an interface
(the rpcbind / rpcallowip options) in a way that is not completely backward
compatible, and therefore needs to be tagged as a major version
in accordance with Semantic Versioning.

However, for practical purposes, this version is more like a minor release,
containing corrections and improvements such as:

- RPC related bug fixes and output improvements
- Performance optimization for getblock
- A new benchmark performance bisection script
- Some support code for future UTXO commitments
- Minor documentation updates

Users who are running any of our previous releases (24.x.y or 25.0.0)
are encouraged to upgrade to v26.0.0.

## Usage recommendations

If you have been using the `rpcallowip` or `rpcbind` options, please
see the 'Modified functionality' subsection of this document to take
changes into account in your configuration.

## Network changes

A few RPC calls have been modified to support additional output fields.

## Added functionality

A new verbosity level (3) has been added to `getblock` RPC command.
It is like level 2 but each transaction will include inputs' `prevout`
information.  The existing `/rest/block/` REST endpoint is modified to contain
this information too. Every `vin` field will contain an additional `prevout` subfield
describing the spent output. `prevout` contains the following keys:

- `generated` - true if the spent coin was a coinbase
- `height`
- `value`
- `scriptPubKey`
- `tokenData` (after May 2023 upgrade, appears if the transaction input had token data)

## Deprecated functionality

None.

## Modified functionality

- The `rpcallowip` option can no longer be used to automatically listen on all
  network interfaces. Instead, the `rpcbind` parameter must be used to specify
  the IP addresses to listen on. Listening for RPC commands over a public
  network connection is insecure and should be disabled, so a warning is now
  printed if a user selects such a configuration. If you need to expose RPC in
  order to use a tool like Docker, ensure you only bind RPC to your localhost,
  e.g. `docker run [...] -p 127.0.0.1:8332:8332` (this is an extra :8332 over
  the normal Docker port specification).

- The `getblock` RPC command has been modified: verbosity level 2 now returns `fee`
  information per transaction in the block.

- The `getblock` RPC command with verbosity level 0 now takes a faster path when returning raw
  block data to clients. It now skips some sanity checks, and assumes the block data read from
  disk is valid. Clients that read this serialized block data via this RPC call should
  be aware that it is not as stictly checked as it was previous to this release, and that
  it's now possible (in theory at least) to retrieve a corrupted block via the RPC interface.
  In practice, it's almost always the case that blocks are not corrupt on-disk and this changed
  guarantee yields significant performance improvements for RPC clients retrieving raw block data.
  Node admins can disable this new fast-path behavior by using the `-checkblockreads=1`
  configuration option, which will enable extra consistency checks for raw block reads via RPC.


## Removed functionality

None.

## New RPC methods

None.

## User interface changes

The semantics of the `rpcallowip` option have changed - see 'Modified
functionality' subsection above.

## Regressions

Bitcoin Cash Node 26.0.0 does not introduce any known regressions as compared to 25.0.0.

## Limitations

Nothing new to report.

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

None.

### Removed documents

None.

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

The semantic changes to `rpcallowip` have been made in consideration
of increased node security, as listening for RPC commands on public
network interfaces is insecure and should be disabled.
See the 'Modified functionality' section above for details.

#### Interfaces / RPC

- 1592a894a0dc0e7fb318c795babda03e7f3b95ff Don't log on success by default in ReadRawBlockFromDisk with `-checkblockreads=1`
- 751bb58b60e4355ef825bc9106e12454c5b1d2b1 Fix getblockstats feerate unit in the RPC command help
- 931fe7280dc48273fda3dc1540f0ba605e0f1519 Nit: Some formatting nits in rpc/rawtransaction.cpp, `getrawtransaction`
- b8c237ad0750f403efc2ae948a329732f333170e rpc: Add level 3 verbosity to `getblock` RPC call; Add `fee` to level 2
- cf7e12ce889786a5f9e3ca5a2a01683339efc1a6 Improvements to `getrawtransaction` with verbosity=2
- eb0b086aea2ec26ca9257eb4598a2702d6a94bac Nit: More nits for getrawtransaction in rpc/rawtransaction.cpp

#### Features in internal development: support for UTXO commitments

- 718e29e80e3ec96b17fa4dc5b00c05c82de8e202 Added new functions to secp256k1 for serializing the pubkey from a multiset
- ebc2a168bde73a0f1bf33efa5c90d808e70ccd64 Modify the generic code in `hash.h` to support alternate hashers
- eeab3ac8e7813e799898fe7bd847f16483111d34 Added C++ wrapper for EC multiset, plus tests

### Data directory changes

None.

#### Performance optimizations

- 4c30a7fcb6d9e0d042773eb3aaba89d16f595d34 RPC: Read block data directly from disk in `getblock verbosity=0`
- 7ae7d42f9452323c8235570b98e144ac4e7abe6c Remove spurious lock
- 8ba475f4c6a2e800859b52b4fe2446ebcfcf6ec3 Update chainparamsconstants.h for version v26.0.0
- e1d39e5c4d143a3561e8fbd1fe9fb64f8d567ff8 Update checkpoints for v26.0.0 (main, test, test4, and chip)

#### GUI

None.

#### Code quality

- 3be863dee2f06b556fbe53782a1b9051f73d6ad3 Some NITs (follow-up to MR 1600)
- 5379d1c67445c7f3e1f2c2a90af3183c3c7c55dc Minor nit: Fixed a typo and a formatting error in span.h
- 85728782f77b0eebd13f57e770bc975da89c084a Nit: Use CURRENCY_UNIT in `getblockstats` help rather than BCH
- 8757d2ae1413615c4911af235b978f24443b1e7e Fix bug: RPC & REST API sometimes reads from ::ChainActive() without locks
- 8dc6789e7a5c95543ae25ae57ec215529c075b00 httpserver: Address review comments from !1429
- 9b5d13cb5aec78a5240b6befbf17816dd88b423a Add additional sanity checks and a unit test for ReadRawBlockFromDisk()
- a37951a013368c59690acc8a48799911f2c3d6df Fix bug where `getrawtransaction` with verbose=2 on a coinbase txn fails

#### Documentation updates

- 4f1ed0d6858a918c37b9c6a19dc96eaa6abc3912 [doc] Update the release version for signature verification instructions
- d0308cef252ecfd3bfaad55b91b7f0ab2bc56ea6 Add rpcthreads to example file.

#### Build / general

- 00202a246716ff50ca2acbfcecc700500e444f3d build: Check for dependency 'help2man'

#### Build / Linux

- 9b6d19c67bbfee46e261777d1c92bedeac7b4228 Fix breaking `ninja check-lint` on my system after latest flake8

#### Build / Windows

None.

#### Build / MacOSX

None.

#### Tests / test framework

- 24479388842befdc54751d4eb8b00e1ad91150fd Fix for rpc_blockchain.py intermittent failure.
- 4355d67db96fcef6a46cc5f79dda30081405081d Fix potential UB in wallet test fixture
- 48d8116976cbdf3898187fb39c1bb3d82b634a13 Prevent theoretical UB for CConnMan and PeerLogicValidation mis-use in tests
- 5256867daae1277108fafd7b629490b80405c504 [fuzz] Avoid double and triple copying of data when running fuzzers
- d7273ec070786b2eadbde50dad77a2b86be5c890 Tests for C++20 work-alike Span
- f3e488a853f3c02e45c6eb494c213bfab5e44537 Temporarily disable payment server tests which fail due to expired certificate

#### Benchmarks

- 61413d0c6162e7293b5467cb07cddd94762cc03c Benchmark bisect script improvements

#### Seeds / seeder software

- 88f1ac6f45860aa1cc44e3862466acdb0778318c Update regex in makeseeds.py to accept BCHN v26.x.x
- e8a8d5f1a71924b767bdbdd2b1e6f93f0d4fa8d0 Updated seeds for mainnet for v26.0.0

#### Maintainer tools

None.

#### Infrastructure

None.

#### Cleanup

None.

#### Continuous Integration (GitLab CI)

- 25bdba4f4732b118af86b3719c3665244f632398 [ci] Add sanitizer job for undefined behavior

#### Backports

- 44830d4cc984aa39d9fe4fc05df51d991deb7d15 [backport] net: Always default rpcbind to localhost, never "all interfaces"
- 50b7cb4fe3c242a5c147ca8a858ee758023c3483 [backport] tests: Modify rpc_bind to conform to #14532 behaviour.
- af4a29dd844f7347fa77bd70e9e9587542d5f083 [backport] CNetAddr: Add IsBindAny method to check for INADDR_ANY
- e07f97db146e61bbc8a8ab018cd09c6b73e86494 [backport] rpcbind: Warn about exposing RPC to untrusted networks
- 14cab1395fc845c2b3f2c654e5a4f18958dc16df [backport] net: fix use-after-free in tests
