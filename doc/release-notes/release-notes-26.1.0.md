# Release Notes for Bitcoin Cash Node version 26.1.0

Bitcoin Cash Node version 26.1.0 is now available from:

  <https://bitcoincashnode.org>

## Overview

This release of Bitcoin Cash Node (BCHN) is a minor release.

It includes mostly bug fixes and performance enhancements,
but there are some new features too, such as new fields in
`getpeerinfo` RPC and new syntax variable for `-walletnotify`.

## Usage recommendations

Users who are running v25.0.0 or v26.0.0 are encouraged to upgrade
to v26.1.0 as it includes checkpoints and updated network static seeds
for the recently completed (15 May 2023) network upgrade.

## Network changes

Unsolicited `ADDR` messages from network peers are now rate-limited to an
average rate of 0.1 addresses per second, with allowance for burts of up
to 1000 addresses.
This change should have no noticeable deleterious effect on the peer-to-peer
network for well-behaved peers, and was merely added as a performance
optimization.

## Added functionality

None.

## Deprecated functionality

None.

## Modified functionality

- The `getpeerinfo` RPC method results now include two new additional data
  items for each peer: `addr_processed` and `addr_rate_limited`.
  See the `getpeerinfo` RPC help for a description of these new items.

- The `-walletnotify` option now understands an additional %-token: `%w`,
  which it will replace with the wallet name when executing the external
  command. For additional information see the help `-hh` for `bitcoind`.

- The current limit of 16 parallel script/sig verification threads,
  controlled by the `-par` configuration option, is substantially increased,
  to a maximum of 256 threads, with the following rules:

   - For users that do not use the `-par` argument/config, current legacy
     functionality is preserved (clamped to 16)
   - For users that set `-par=0`, their number of cores will be used (clamped to 256)
   - For users that set `-par=N`, with N a positive integer, the parallelism will
     be set to N, clamped to 256; note that users can set this higher than their
     core count, which preserves current functionality
   - For users that set `-par=N`, with N a negative integer, the parallelism will
     be set to leave N threads free, clamped to 256
   - For users using the GUI, the maximum number of threads they can set is equal
     to the number of cores, and is no longer 15 (sic!)
   - GUI users will not be able to set this limit to a number higher than their
     number of cores, via the slider.

   An upper limit is still needed, otherwise users can create thousands of threads
   and crash the node.

   The old limit of 16 was set 10 years ago when this functionality was first introduced.
   Since then both hardware and software have progressed vastly. Users should
   be able to set the parallelism higher if their hardware supports it.
   There may be diminishing returns setting it to higher than 16, but verification
   times do improve with more cores to an extent on modern platforms.

   NOTE 1: Current functionality is preserved only for users that don't use `-par`.

   NOTE 2: Only headless users can set `-par` above their core count.

## Removed functionality

None.

## New RPC methods

None.

## User interface changes

- A bug in the fee rate override in the Send dialog of the GUI has been fixed.
- The `dumpwallet` file header comment now refers to "Bitcoin Cash Node",
- The `importwallet` progress bar behaves more sanely.
- Importing very large wallets has been made significantly faster.

## Regressions

Bitcoin Cash Node 26.1.0 does not introduce any known regressions as compared
to 26.0.0.

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

- Building / testing on current Arch Linux requires some additional package
  installations which are not yet reflected in the documentation.
  (see Issue #477).

- A GUI crash failure was observed when attempting to encrypt a large imported
  wallet (see Issue #490).


---

## Changes since Bitcoin Cash Node 26.0.0

### New documents

None.

### Removed documents

None.

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

- 31ed5aa7f2c99624a6eb92e7dfc8828612ce9bd1 Fix issue with peers not receiving DSProofs if they ever use the mempool p2p message
- 5e8b41ab20a03917a74332487f75d7feb464e6d3 Rate limit the `Broadcast` signal that gets spammed in SendMessages
- e6615382032cba12e1c499a7fc9a5679693b61ce [net processing] ignore unknown INV types in GETDATA messages

#### Interfaces / RPC

- 39f8dfc4b24d38370776afa3da415fc674d4faa6 wallet: Replace %w by wallet name in -walletnotify script
- 53e7d8055ab9c977adc7094585cefd2f1c8d40cc RPC: Fix `dumpwallet` comment/header to say "Bitcoin Cash Node"
- b8df87882b24a6905915d7129841ea134c6bda65 Remove limit of 16 from -par

#### Features in internal development: support for UTXO commitments

- 3f8b0b02ae6b0aca4b71fb3138024c490f50e341 Two small quality of life improvements (needed for utxosync work)

### Data directory changes

None.

#### Performance optimizations

- 1ae0bb2b2f8911ee242dc74bacc55b281f08b529 Performance fix: Modify logic slightly for the msghand thread sleeptime calc.
- 2b3ce0a7b6aa457c4aa6f4c98f067ad2825e885c Optimize two block reads with raw version
- 351daeebe0ebaf61f718a6f95008f9da92b8b4bd Batch writes to wallet for rpc `importwallet` for faster imports
- 7ee3ad0ee65e40156803b766734f3d7f1d3464e6 Add checkpoints post-upgrade9
- 86dd3ef3349d1a3fc7f6c54e4cfa3c37067cb235 Remove repeated calls to the slow gArgs object in perf. critical network code
- 9645fc84c63e9ca32fa730ee5c9277214af1c7e6 Update chainparams assumevalid and minimumchainwork for 26.1.0
- a92f06eb391f1299a54978afabe78b2c5f7aad8e Remove redundant calls to `GetTime*()` in msghand thread
- f831cdb2ee75548f43f1eec0be484cbd512ad57d Reduce redundant copying for CCheckQueue (script check threads)

#### GUI

- 59f9780fb4008b6bac8e4737746f50bef602ad29 Fix the fee rate override flag not being set when Send invoked in GUI
- 93805bae3ba896216e7acfbe13c10dc2bb1c9dac qt: Fix potential crash due to mis-use of showProgress
- ca155b3e783a4801966ba27117fcc1fc75bb6d2f [qt] Improve importwallet progress display

#### Code quality

- 2793192d55a505be6de2ea1ec8caad96f5eb31f3 refactor: remove references to deprecated values under std::allocator
- 3fa31054c810db69ff65d56cb7b80730d02dda63 trivial fix: Ensure HeapOptional move-constructor actually moves
- 428963c83fadea27586694e4701012832e3f50a1 Refactor and docs for ProcessGetData
- 572676d4be2088ceba2d7ae30868c627299ba225 nit: Use emplace_back in favor of push_back in CNode::ReceiveMsgBytes
- 5a20c2c7d40015df2c847abb0838e96a7522d678 Modify method `HTTPReply::WriteReply` to use `Span<const uint8_t>`
- 5bf58b243e6c2940151d40fe86187020da513055 Reduce redundant copying in the `CScheduler` class + other nits
- 5e8ed5a78f92f001f9a6d07bd956009b638a0a41 Update copyright year to 2023
- 7d840a66d394eec901d0f4da29aeb9234a5d2c6f Fix missing includes on GCC 13
- af2e45243fa59ebee46b3029a0ae0d0fa3923f69 Annotate some blockstorage & validation functions & data for thread-safety
- b549364a93258aa94c2c713bd722bce2511fb396 qa: Fixed a typo in `synch.cpp`

#### Documentation updates

- 0b0a0359e80b1380a96a6e0b6d0baeb67c68555e Update release notes to talk about `-walletnotify=` and `%w`
- c61f448d1e0456315501503cdc84ce5df4519643 [doc] Update testnet fork heights to reflect upgrade 9


#### Build / general

- b585ff89b0bb6f7011b887caaa02f36c75dc393b [qa] Bump version to 26.1.0

#### Build / Linux

- 6b75a72283fd269a3fe26e348b88f564923bc398 Fixes build errors on GCC 13
- aca0bc94a5d525df914d1dbead30257ff6285776 Fix for build failing on latest arch
- fcac1db37124516e7f7c56bc98fdd1dc1a8ce915 [cmake] Fix a potential version mismatch in BerkeleyDB

#### Build / Windows

None.

#### Build / MacOSX

None.

#### Tests / test framework

- 533611f85b1d7f6f81185ac2ccb101ccef6fdd80 Fix for failing unit test
- d9df0e56c17537f85d1bb6522f52e9e6d26811c1 test: Check wallet name in -walletnotify script
- e016eb31b1d68382f4c98997d747729fb3245761 Fix intermittent "fee" related crash in `rpc_blockchain.py`

#### Benchmarks

- 715fe8e97ca6e41a55044542827bbe6ef83501c5 Update benchmarks in response to new verbosity mode `TxVerbosity::SHOW_DETAILS_AND_PREVOUT`
- 8b60b41e24bba2906e249764df4125b2e610be38 Add benchmark to test the speed of CNode::ReceiveMsgBytes
- 90ee32334a0e49ffa8932b95e0f43c53184d1711 bench: Refactor blockdata class

#### Seeds / seeder software

- 5bd1b9c5e458107d6ccac8ebca952fa02952baad Update regex'es in makeseeds.py after May 2023 upgrade
- 873f6dcdd9f8450d6a941209f17ce482d7968ad4 Update static seeds for mainnet

#### Maintainer tools

None.

#### Infrastructure

None.

#### Cleanup

- 35d3bd208e1dc680e4c191d890a9d86b24dd439b Remove unused `RPCSerializationFlags` functionality
- 65d2acb402f35361f315043cd870067ec4ce8fd5 Remove "zero after free" allocator from codebase (used by CDataStream)

#### Continuous Integration (GitLab CI)

None.

#### Backports

- 0b2831161ac386048b4241ed84f7972dd6b17e94 backport: Disable unused special members functions in UnlockContext
- 1887c10bf2eafd7c7f5b7489f2edeeec47c42fd3 [backport] Rate-limit unsolicited p2p ADDR messages
- 398f0c0c3bdaedc18dab99fef3ad1ad2390d17ad backport: Fix potential use after move in validation.cpp
- 8c0de7d1470cd059361909c6c8b9a07db636d313 backport: Give WalletModel::UnlockContext move semantics
- cc520af296b1e78f507b8c5272021b73004b9915 [backport] Create blockstorage module
