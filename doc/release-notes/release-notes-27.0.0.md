# Release Notes for Bitcoin Cash Node version 27.0.0

Bitcoin Cash Node version 27.0.0 is now available from:

  <https://bitcoincashnode.org>

## Overview

This is a major release of Bitcoin Cash Node (BCHN) that implements the
[May 15, 2024 Network Upgrade](https://upgradespecs.bitcoincashnode.org/2024-05-15-upgrade/).

This release implements the following consensus CHIPs:

- [CHIP-2023-04 Adaptive Blocksize Limit Algorithm for Bitcoin Cash](https://gitlab.com/0353F40E/ebaa) commit `ba9ed768` (19 Nov 2023)

This version contains various additional minor corrections and improvements.

Users who are running any of our previous releases (25.0.0 or v26.x.0)
are urged to upgrade to v27.0.0 ahead of 15 May 2024.

## Usage recommendations

The update to Bitcoin Cash Node 27.0.0 is required for the May 15, 2024
Bitcoin Cash network upgrade.

## Network changes

This major release implements the Adaptive Blocksize Limit Algorithm for
Bitcoin Cash, which is a change to consensus rules that allows the maximum
block size to gradually increase and decrease based on how full blocks are.

## Added functionality

- Added configuration option `-percentblockmaxsize` which is an alternative to
  `-blockmaxsize`. `-percentmaxblocksize` can be used to configure the node's
  maximum mined block size as a percentage of the maximum block size for the
  network. So for instance the default on mainnet is `-percentblockmaxsize=50.0`
  (16 MB). Can be any value from 0.0 to 100.0.

## Deprecated functionality

- While 32-bit builds are still supported in this release, they are deprecated
  and planned to be removed from BCHN after the May 2024 upgrade.

## Modified functionality

- The -excessiveblocksize configuration value has modified semantics pre-upgrade
  vs post-upgrade. After ABLA activates, it acts as a floor value as the base
  "minimum" max block size.
- Added an additional returned data item to the `getmininginfo` RPC:
  "miningblocksizelimit", which is the block size limit used for mining as
  configured for the node by e.g.: `-blockmaxsize`.
- The Qt GUI RPC console now is fixed to match the HTTP RPC with respect
  to stateful "config related" commands. Commands such as `getmininginfo`
  and `getexcessiveblock` now match the actual node config (and the HTTP RPC).
- The default size cap for mining blocks has been set to 50% of the capacity
  on networks where it has previously been set to 25%. This effectively
  increases the soft (mining) blocksize default from 8MB to 16MB.
  Mining pools that wish to generate bigger or smaller blocks can still
  adjust the configuration option as usual.

## Removed functionality

None.

## New RPC methods

A 'fillmempool' RPC method has been added for regtest.

## User interface changes

- The new configuration option `-percentblockmaxsize` has been added. It is
  described in more detail in the "Added functionality" section above.

## Regressions

Bitcoin Cash Node 27.0.0 does not introduce any known regressions as compared
to 26.1.0.

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

## Changes since Bitcoin Cash Node 26.1.0

### New documents

None

### Removed documents

None

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

- 286ca4e9ce4a3a127b89f0711b6b8bc0f3819762 Implement CHIP-2023-04 Adaptive Blocksize Limit Algorithm

#### Interfaces / RPC

- 37f9f474c5a1f396bc4fed20e5fb3d3fc8e94513 Bump node expiry to May 15, 2025, introduce tentative "upgrade11"
- a7b52f819182b4693c21bc520b17405f3252df14 Add the RPC `fillmempool` (regtest only)
- b110d07a901543f522c3797dd421a023c18a6e5b Added new CLI arg `-percentblockmaxsize` as an alternative to `-blockmaxsize`
- e4bc71dfb9caaf1c5dc917309db7c228e1488634 Add the ability to see the configured `-blockmaxsize` from the RPC

#### Features in internal development: support for UTXO commitments

None

### Data directory changes

None

#### Performance optimizations

- 40e3d6ce6c2880a8af982b2b13970ad706370af7 Update the codebase to do a height-based check for upgrade 9
- 6877945346bc2a7ce6b2be1033d7b7e4b9a91843 Increase the default mining block sizes
- a130eb09b48d39a5794f8af7a0e68545a7bebec7 Add ReadBlockSizeFromDisk() function to blockstorage.cpp
- bedee413c700b2ca2cd7599e28e9ccf325712f06 [qa] Update chainparams assumevalid and minimumchainwork for 27.0.0
- e635da2b4f91319ffc1d648d7385324ff2c4bb40 [qa] Update network checkpoints

#### GUI

- 24bd12154e6bb527ce69510a75cbbf45b5984839 Alternative fix for Qt GUI RPC Console using an "ephemeral" config object

#### Code quality

- 0eaa947c39b874944ce8b208f667f83694a666a0 Code quality nit: Fix a G++ 12 warning in blockstorage.cpp
- 45ad5a4a2cb34cc05b0830037942727c5a2a1efc Make class CAutoFile move-assignable and move-constructible
- 4624f046484200b1b8c118b135894d3122c9f6b9 Compiler warning fix: Remove unused variable in policy.cpp
- 7b781217f8722436d6ed437502d4d889fbb1904a Trivial: Fix two typos in comments in miner_tests.cpp
- 7f2b83aca1d0819da2d3539616246571e997b9a6 [qa] Bump version to 27.0.0, rotate release notes
- 94433b8e3972f39418f12566f7fd0323c2f4b164 Add support for generic serialization of std::optional<T>
- 959d6ed06f4a43f3742214bd375eb3c2dcdaaf22 Add `GetNextBlockSizeLimit()`, refactor `BlockAssembler` to use it
- a395e319eea1cf52a6ed4a07bacd91df9c9a5c79 Fix comparing wrong blocksize parameter variable
- b7110f8aca0e320f8c716ffaa6c57340eeee6176 [qa] Update blockchain data sizes for mainnet for v27.0.0
- bf9c3c754c69daa5ee80c3587932defd9b734335 Rename the "excessive block size" concept in the C++ codebase
- c8bcd188937c9ca6fbb5c7c499a87e4d190056d3 Compiler warning fix: Remove unused variable in txmempool.cpp
- ca48aa4c644282935c1ff868e24f03aab8ea828a [qa] Update copyrights across the codebase
- d0f989cb9954d9743430b49215042f18df7695a2 Add Upgrade 10 "Activation" API and unit test
- fa886d49c7177a4ac5d2e9facc6d99bd9f0da315 Remove `const_cast<Config &>` from various places in code.

#### Documentation updates

- 73e413cdf4a60d389e933a3d140713e5692f8ac9 [doc] Update Clang formatting tools documentation
- 79c8650d229d191cae2aeb59b4cccefe05589534 [qa] Update test network document to reflect testnet3/4 remain fixed
- b46cec23212b10682928a20d657eb1025a43d28f [doc] Update header include guard documentation
- e3181397524c4fabcbafee026dd6fe81621f63cb [doc] Add ABLA CHIP to BCHN's BCH upgrades document

#### Build / general

- ebed3da1dc7f60abd5f0e1ca6278b79ad6132b66 Added HAVE_INT128 test and variable to bitcoin-config.h

#### Build / Linux

- 2adefbfc7f6a0225f32fb7f8823d257c4a8a51c4 build: Support building inside of a git worktree

#### Build / Windows

- 9a767b3c9db219c772d53292a65e2cd8f2d52211 [build] Remove unnecessary _Event_WINDOWS_LIBRARIES

#### Build / MacOSX

None

#### Tests / test framework

- 67ca3f406f42d155adacaa5d247456ef79649542 [tests] fix PEP 484 implicit optional error
- 88300e6d2d34e1e820ad15fff36f644eb5f9e62a Add a unit test for the Config class's "percent max blocksize" setter/getter
- f4a906bd05da68aac0a99b622764dd8354c277d8 test framework: Pass down the rpc_timeout to wait_until on node restart

#### Benchmarks

None

#### Seeds / seeder software

- 0521923f432cf4abc978510785e94e775ea1c1f8 Update regex'es in makeseeds.py ahead of May 2024 upgrade
- 2c08e4c74f10a10f873feaa2ca44e46936655f78 [qa] Update static seeds for some test networks
- 43a1620e306d0567b6b937ac05e58ac1941234ed [qa] Update mainnet static seeds for v27.0.0 release
- 55f5916401fb474c44d15b2d962928bd34856b88 [qa] Update static seeds for chipnet test network

#### Maintainer tools

None

#### Infrastructure

None

#### Cleanup

- 66442fd03829c0ce9667533b03546134095324b9 [qa] Update freetrader public key (expiry extension to 2025-06-07)

#### Continuous Integration (GitLab CI)

- 64fd5f891fe1a02875b0e16f088862b6312423a9 [ci] Pin the pymdown-extensions dependency to v10.2.1
- 9300b882e022085272e3090bb0349110fdbbe8e7 CI: Save more sanitizer-undefined artifacts on failure

#### Backports

- 1ae5ca204e1104a0f2f6b46ae93eedb307196379 [backport] refactor: Drop `owns_lock()` call
- 34301c7128f505fef08712527e3ab522a9eeb906 [backport] build: libevent 2.1.12-stable
- 8e889ee9c9e2830cf77a3d2f59cf652b4e4e9b3b [backport] refactor: Do not discard `try_lock()` return value
- b9a103d2f0a8b17952e4829af570b29013a91bdc [backport] [cmake] Use the protobuf supplied cmake file instead of the cmake supplied one
- c97419daeb7907dcad3958a9dcbe0939a10fd587 [backport] build: suppress array-bounds errors in libxkbcommon
- fc9c13e97036c5a8619cdfa310371cc765684052 [backport] bump libevent to 2.1.11 in depends

