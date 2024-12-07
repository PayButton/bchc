# Release Notes for Bitcoin Cash Node version 25.0.0

Bitcoin Cash Node version 25.0.0 is now available from:

  <https://bitcoincashnode.org>

## Overview

This is a major release of Bitcoin Cash Node (BCHN) that implements the
[May 15, 2023 Network Upgrade](https://upgradespecs.bitcoincashnode.org/2023-05-15-upgrade/).
This release implements the following consensus CHIPs:

- [CHIP-2021-01 Restrict Transaction Version](https://gitlab.com/bitcoin.cash/chips/-/blob/3b0e5d55e1e139046794e850287b7acb795f4e66/CHIP-2021-01-Restrict%20Transaction%20Versions.md) v1.0
- [CHIP-2021-01 Minimum Transaction Size](https://gitlab.com/bitcoin.cash/chips/-/blob/00e55fbfdaacf1436e455289086d9b4c6b3e7306/CHIP-2021-01-Allow%20Smaller%20Transactions.md) v0.4
- [CHIP-2022-02 CashTokens](https://github.com/bitjson/cashtokens) v2.2.1
- [CHIP-2022-05 P2SH32](https://gitlab.com/0353F40E/p2sh32/-/blob/f58ecf835f58555c9087c53af25da92a0e74534c/CHIP-2022-05_Pay-to-Script-Hash-32_%28P2SH32%29_for_Bitcoin_Cash.md) v1.5.1

This version contains further corrections and improvements, such as:

- Support for new 'chipnet' test network (for pre-upgrade testing)
- Double Spend Proof notifications in the GUI wallet
- BIP69 input/output sorting in bitcoin-tx command line tool
- Enhancement for UI tests in test framework

Users who are running any of our previous releases (24.x.y) are urged
to upgrade to v25.0.0 ahead of 15 May 2023.

## Usage recommendations

The update to Bitcoin Cash Node 25.0.0 is required for the May 15, 2023
Bitcoin Cash network upgrade.

While there are no known regressions in this version at this time, as
always we recommend that all users inform us of any potential regressions
via our support channels.

Existing users, whether miners, exchanges or personal wallet users can
expect to continue status quo operations without disruption by simply
swapping binaries and restarting the node before May 2023, as long as
they do not need the additional functionalities of Cashtokens and P2SH32.
While Cashtokens and P2SH32 usage will already be possible after activation
through custom software, exchanges looking to gain these capabilities may
want to watch for a future release of BCHN that includes better support in
the bundled wallet and RPC commands.

No post-activation tokens are going to be accidentally lost for any legacy
operators who do not take any additional steps other than upgrading their
nodes to this release.

Block explorer operators may begin to see transactions of a new and unfamiliar
format in Cashtokens after upgrade activation. While these transactions
should not impede status quo operations, operators may wish to modify their
software in order to interpret the additional token information they carry.

This client release has a built-in expiry date of April 15, 2024,
ie. a month ahead of the (tentative) next upgrade after May 2023.
The expiry mechanism causes upgrade warnings to be issued, and
preventively disables RPC if the software is run past the (tentative)
scheduled upgrade date of May 2024.

## Network changes

### New test network supported: chipnet

A new `chipnet` test network has been added to enable testing of all
the new upgrade features (such as CashTokens, P2SH-32) ahead of the
mainnet upgrade.

This network can be accessed by running with the `-chipnet` option.
It has activated the new features already, 6 months early, on
15 November, 2022.

Technically, it is a chain fork of 'testnet4', with much the same qualities.
Its default ports are 48333, 48332, and 48334 for p2p, rpc, and onion
respectively.

It retains the same cashaddr prefix as the other testnets: `bchtest:` and
the same legacy address checksum bytes. Like testnet4, it defaults to 2MB
blocks and defaults to requiring standard txns for relay.

Further parameters are described in more detail in `doc/test-networks.md`.

Historical discussion on its creation can be found here:

https://bitcoincashresearch.org/t/staging-chips-on-testnet

Seeders for this network should be started with -chipnet, and bitcoind
should be started with `-chipnet` and also have a section `[chip]` for the
new network.

## Added functionality

### 32-byte Pay-to-Script-Hash (P2SH-32)

P2SH-32 (pay-to-scripthash 32) support has been added and consensus rules
for this feature will activate on May 15, 2023.

This is a feature that provides more protection to certain smart contracts.
It is currently only usable through the RPC interface, and sending is not
enabled by default in the wallet functionality or accessible via the GUI,
except through RPC commands. Receipt of P2SH-32 transactions is supported.

For more information on P2SH-32, see:

- [CHIP-2022-05 P2SH32](https://gitlab.com/0353F40E/p2sh32/-/blob/f58ecf835f58555c9087c53af25da92a0e74534c/CHIP-2022-05_Pay-to-Script-Hash-32_%28P2SH32%29_for_Bitcoin_Cash.md)
- Discussion: https://bitcoincashresearch.org/t/chip-2022-05-pay-to-script-hash-32-p2sh32-for-bitcoin-cash/

### 'sort' command added to bitcoin-tx

The 'sort' command can be used to sort inputs and outputs according to
BIP69.

Note that sorting usually requires re-signing (similar to other commands
that mutate the transaction in some way).

BIP69 sorting is enabled by default in the wallet application.

### Visual indication of transactions involved in Double Spend Proof

Upon arrival of a Double Spend Proof (DSProof), an application notification
will be shown, additional information will be shown in the transaction list
and transaction info window.

DSProof related transactions are indicated with a red color and an exclamation
mark symbol in the graphical user interface.

Note: The GUI does not store information about transactions that have been
double spent in the wallet.dat file. A restart of the GUI therefore clears
the history and these transactions will look 'normal' again.

## Deprecated functionality

Currently, standard relay rules restrict transaction versions to either 1 or 2.

After May 2023 upgrade, transaction versions will be restricted to be either
1 or 2 as a consensus rule, i.e. strictly enforced.
Therefore, applications that create transactions should not generate
transaction versions other than 1 or 2, otherwise their transactions will be
rejected after the upgrade.

Note: This is perhaps not strictly a deprecation, since we are not aware of any
applications generating other transaction versions, but since consensus is being
tightened up here, it seems prudent to mention it.

## Modified functionality

### New cash address formats for token-aware P2PKH and P2SH

Two new "address types" have been added to the cash address format.
These are described in the CashTokens v2.2.1 specification.

- TOKEN_PUBKEY (token-aware p2pkh, type=2)
- TOKEN_SCRIPT (token-aware p2sh, type=3)

The "address type" is encoded into the cashaddress string.
Thus wallets wishing to signal token awareness will be required to
advertise their addresses using these two new "address types",
making token-aware addresses distinct from the plain cash addresses.

For example the following p2pkh:

```
bitcoincash:qr6m7j9njldwwzlg9v7v53unlr4jkmx6eylep8ekg2
```

Could also be encoded as signaling token-awareness and it would look
like this:

```
bitcoincash:zr6m7j9njldwwzlg9v7v53unlr4jkmx6eycnjehshe
```

Both of these addresses end up mapping to the same locking script.

Notes:

Token-aware addresses are not yet supported in the wallet database
(wallet.dat).
The bitcoin-qt wallet (and the RPC wallet) store addresses internally
in the wallet.dat as "legacy" strings thus the "token awareness" of
an address you sent to is "lost" in the UI or in RPC wallet history.
So if one were to send to a 'z' address, when reading back the history
one would be told it was a regular q address (same destination).
This limitation will be removed in subsequent release to bring more
token support.

The node wallet itself does not advertise these new "token aware"
addresses when you ask it for a new receiving address, since at the
present time the wallet cannot actually manage and send tokens.
The CashTokens specification says only wallets that can send tokens
may advertise these new type=2 and type=3 addresses. In future work
we may add support for managing and sending CashTokens, at which time
the wallet UI and RPC would be updated to advertise "token aware"
addresses.

### 'getdsproofscore' DSProof scoring RPC

The 0.25 scoring criterion of the `getdsproofscore` RPC has been extended
to trigger if a transaction or any of its in-mempool ancestors happens to
have a P2PKH input that is not signed with SIGHASH_ALL or is signed with
SIGHASH_ANYONECANPAY (which indicates very weak DSProof protection).

As before this release, a value of 0.25 may also indicate that up to
the first 20,000 unconfirmed ancestors were checked and all have no proofs but
*can* have proofs. Since the transaction in question has a very large
mempool ancestor set, double-spend confidence should be considered
medium-to-low.
This value may also be returned for transactions which exceed depth
1,000 in an unconfirmed ancestor chain.

### Two error messages have changed:

- If the node receives a transaction that spends an UTXO with a locking
  script >10,000 bytes, the node now generates the error message
  `bad-txns-input-scriptpubkey-unspendable`.
  Previously it would generate the message
  `mandatory-script-verify-flag-failed (Script is too big)`

- Similarly, if the node receives a transaction that spends an `OP_RETURN`
  output, the node now generates the error message
  `bad-txns-input-scriptpubkey-unspendable`.
  Previously it would generate the message
  `mandatory-script-verify-flag-failed (OP_RETURN was encountered)`

The reason for this change is that sometimes an oversized locking script
ended up "looking like" an `OP_RETURN` when read from the UTXO database.
In that case, the previous behavior was inconsistent because it depended
on whether the output being spent was consuming an output still in memory
or coming from the UTXO db.  Thus, it was possible to receive the
`OP_RETURN`-related error message when actually attempting to spend an output
with an oversized locking script.  As such, the two ambiguous cases have been
collapsed down into 1 consistent error message.

This error message change does mean that the node will now push a different
BIP61 string to its peers when it receives an invalid transaction that
triggers these error conditions.  However, no known nodes actually pay much
attention to the exact format of the error message that they receive when a
transaction is rejected by a peer, so this change should be relatively benign.

### P2SH-32 addresses are now supported by RPC

Certain RPC methods that take an `address` parameter (such as `sendtoaddress`) will now
also correctly parse P2SH-32 addresses and thus will allow composing of transactions and
sending of funds to P2SH-32 addresses, even before the May 15, 2023 upgrade date.

Note that on mainnet such transactions will remain non-standard and cannot be relayed
until P2SH-32 activates on May 15, 2023.

P2SH-32 addresses are longer than regular P2SH addresses, for example:

```
bitcoincash:pwqwzrf7z06m7nn58tkdjyxqfewanlhyrpxysack85xvf3mt0rv02l9dxc5uf
```

is a P2SH-32 address.

Calls like 'decoderawtransaction' or 'getrawtransaction' will now be able to emit this
new kind of address. Software which processes output of these calls should take this into
consideration. Software that is sanitizing inputs should also be extended to anticipate
these P2SH-32 addresses.

### Cash Token information support in RPC interfaces

The following RPC calls have been extended with facilities to transport or accept
information about Cash Tokens:

- `gettxout`: `tokenData` JSON object is present if the output contains
  token data
- `scantxoutset`: a `tok<category>` output descriptor has been added to allow
  matching of token outputs with a specific category (token ID). The call will
  return token data in this case, including the total amount of each fungible
  token.
- `validateaddress`: an `istokenaware` boolean result field indicates whether
  the address type supports Cash Tokens or not.
- `getrawtransaction`: `tokenData` JSON object is present for outputs
  that contain token data
- `decoderawtransaction`: `tokenData` JSON object is present for outputs that
  contain token data
- `signrawtransactionwithkey`: the `prevtxs` object has been extended with an
  optional `tokenData` object containing token data fields. The `sighashtype`
  field has also been extended with combinations including SIGHASH_UTXO (0x20),
  which is a new sighash type introduced in the CashTokens specification, and
  only valid after the upgrade.
- `decodepsbt`: the input `utxo` object has been extended with optional `tokenData`
- `listunspent`: extended by two new boolean options controlling the listed results:
  - `includeTokens` (default: false): whether to show UTXOs with CashTokens on them
  - `tokensOnly` (default: false): whether to only show UTXOs with CashTokens on them
    (implies `includeTokens=true`)
  The `listunspent` result will contain `tokenData` objects for unspent outputs
  that carry token data.
- `signrawtransactionwithwallet`: the `prevtxs` object has been extended with an
  optional `tokenData` object containing token data fields. The `sighashtype`
  field has also been extended with combinations including SIGHASH_UTXO (0x20),
  which is a new sighash type introduced in the CashTokens specification, and
  only valid after the upgrade.
- `walletprocesspsbt`: extended with SIGHASH_UTXO (0x20) sighash type combos.
  SIGHASH_UTXO is only valid after the upgrade.

Please refer to the help documentation for the respective RPC call for more detailed
information on the structure of the token-relevant data objects.

Further support for tokens over more RPC commands are expected in one of the
next releases.

## Removed functionality

The previously deprecated `setexcessiveblock` RPC call has been removed.
Please use the `excessiveblocksize` configuration setting instead.

## New RPC methods

No new RPC methods have been added, but certain RPC methods have been
extended to support CashToken information and to accept P2SH-32 addresses.

Please refer to above sections on 'Modified Functionality' for details.

## User interface changes

Several RPC calls have been extended to support token data.
These are listed separately in the section above titled
'Cash Token information support in RPC interfaces'.

### GUI: notification and indication of double spends

The above section 'Visual indication of transactions involved in Double
Spend Proof' describes the new GUI notification and indication of
DSProofs.

### New client option `-allowunconnectedmining`

This option defaults to false which preserves previous behavior.

If this option is set to true, the node will not require that it be connected
to at least one peer node before `getblocktemplate[light]` works correctly.
This is a "debug" option and its help description can only be seen with `-hh`
to bitcoind.

### New client option `-chipnet` (default: false)

This option is used to synchronise with the new 'chipnet' test network.
It defaults to false (same as the other testnet selector options).

For more details on 'chipnet' and other test networks, refer to:

https://docs.bitcoincashnode.org/doc/test-networks

Note: The bitcoin.conf configuration file option is `chipnet=1`
      with a corresponding new section title of '[chip]'.

### New client option `-upgrade10activationtime`

This option does not do much but controls the (tentative) upgrade MTP
time for the upgrade 10 (tentatively 15 May 2024, 12:00 UTC).

This setting also controls the expiration warning time of the client.
This will start warning about expiration due to coming upgrade,
one month ahead (i.e. in April 2024).

### RPC `createrawtransaction` accepts "data" array

The `createrawtransaction` now accepts an array of hex encoded data chunks
each of which is pushed to stack separately.

### RPC `getrawtransaction` extended verbosity argument

The `verbose` argument of the `getrawtransaction` RPC call has been extended
to represent a verbosity level. A new verbosity level 2 is introduced which
allows for prevout lookup, input value determination and transaction fee
calculation.

### New BIP69 'sort' command for bitcoin-tx tool

As described above in the section "'sort' command added to bitcoin-tx",
this add BIP69 input/output sorting capability to the bitcoin-tx tool.

### Uniform dumping stats for mempool and DSProofs

The `debug.log` output for the dumping mempool and DSProof data on client
shutdown have been made uniform and slightly enhanced. This diagnostic change
will not be of interest to most users.

## Regressions

Bitcoin Cash Node 25.0.0 does not introduce any known regressions as compared to 24.1.0.

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

## Changes since Bitcoin Cash Node 24.1.0

### New documents

None.

### Removed documents

None.

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

- 9d8c67ee480bbe13586404422c20d4958893e64b Upgrade9 (May 2023) - Everything plus fixups

#### Interfaces / RPC

- af9f6f3178e16c9cf7ec7cdfa16f1f6a7fa32204 Add CLI option `-allowunconnectedmining` (default false) + small code quality fix
- 64fc83e5189213a0fa85a19b3dcd2c668df32184 [rpc] createrawtransaction accepts "data" array
- e0f70a83506e978d4f9aa29135fce0e450fe141c [rpc] getrawtransaction extended verbosity
- 8647eecaa302ba078cd3a284f8c83aca3b40269e [rpc] Update user-agent string of BCHN in getnetworkinfo RPC help
- 9dee9c365597d52538176f2039670dcbd95d59aa Remove deprecated setexcessiveblock RPC call
- 8c134e30c4182947768fad493f9b2321e7ca8d8c Nit: Uniform dumping stats for mempool and dsproofs
- 5234ededd867e60736b4893479bc1be6ed4f4941 Define `upgrade10activationtime` for `-chipnet` (was undefined!)

### Data directory changes

None.

#### Performance optimizations

- bc439b2327c151e054138534306e869d3bf3a7da Add ReadRawBlockFromDisk to read raw block data without deserialization
- 41e8b5414e0968ef95f893969c4097f90684b5f2 Update chain params (assume valid and minimum chain work)
- 796f7999003131d15ecc99d183f38466c9c7f70b Update checkpoints for mainnet, testnet3, testnet4, and chipnet
- 3d693fc4f870a407cc043f0a5104fa8bdcb55b3b Small performance fix and various code quality nits in `primitives/block.h`
- 2169f980bbba81a8ae6674701f0ae5af9420a834 Reduce unnecessary copying in loop
- 64b7217b8596d77a01236a53e5e378531fb7c339 Fix performance regression in CNetAddr::SerializeV1Array

#### GUI

- be6e22d11b44aa8265e98ba92361079a005b9c3a Add DSP handling in UI wallet

#### Code quality

- 055fb5018a6ff7bd93fe75a65aa97efd279f46d8 Make CBlockIndex assignment operator deleted
- cc863aa6071aa2b795a04e479cccddad04220129 Remove sighash fork value (forkid)
- 4a28e059338da6a703b08246c8f3ed7ea392a1a1 Rename "ForkID" functions and variables to "Fork"
- 6e8331690843a76137855bf48958e3798638eb5c Replace boost::thread with std::thread
- 70115a2211df7350d87f8c7abd92745939beee67 Follow-up to merge of MR1600 -- some small nits
- 1d7c3ed23e833447ad3a4abc546a8a8b8da916c5 Fixed the comment for ContextualCheckTransaction to be more accurate
- cfa4d5b90562172dc19bd81b7350448afd59c42c Code quality: Use global constant `BLOCK_HEADER_SIZE` rather than int literal `80`
- 285e93937ad8d8ef5d81f0a4d818eb53d05f0af9 Boost elimination: chrono, mutex, condition_variable
- 9aa48d2511a345725864f2d4c164ef149a94ca5f Boost elimination follow-up: some modernization and code clean-up
- 0a5fa6246387c3a9498898ee5257ee6950c1b635 Update copyright headers

#### Documentation updates

- 36b9acd8758538400c2dfd053a2ad3f002f8e151 [doc] Add chipnet info and update testnet docs table for previous two upgrades
- db849607f0c2877c2c63fc97a374e4f83d98d805 [doc] Add v24.1.0 release to BIP155 description
- 3109fb2bebe3e60338f6329f3ae3dc40e47f75bc [doc] Call out mkdocs docs-generation dependencies
- 67086b207bf7fe90927853dd72e1fe87f55d0f31 [doc] Clarify general expectation and policy on paid merge requests
- 2364c55e5aa877eff1f55c965279483548d1981e [doc] Documentation fixes Nov 2022
- fd9a57f990e2da341230725b0f76793aae5c67e7 [doc] Fix bitcoin.conf link and its discoverability
- 143970898bffa0ac4ae4171b3d1a8c7ee8e68136 [doc] Fix filename mentioned for mkdocs config
- 9f9aa7f1ce1ac13b9a92d047ec7ac768e6dcff0c [doc] Improve markdown formatting depends/README.md
- 7ea6f869684eb4b55b6408d4685dca26ad63e48a [doc] Point contributors to the main documentation
- 9b16c9925befbca894b2f889d36b95ca9619ef1c [doc] Remove irrelevant benchmarking.md mention
- bb53229c295b25baee7be2757718e325e504709d [doc] Shorten line length
- 231d0ed280201423ae73eb6d52473ac4ba6ce07a [doc] Standardise heading capitalization
- b7ad1c56e97c088c29c0bdcd9b25bdea2af7d24b [doc] Update support channel in docs

#### Build / general

- 4b778adfad6a379c636d9b453388b18b9f075d05 Upgrade Qt version to 5.15.3 for depends & gitian
- d7d33557d759593b7a66cc48a05ac10b93c6b183 [build] Fix build fail for boost 1.80.0
- dad834fe7b22f92ecbeb9d1db02c7122aa60aba6 [qa] Bump version to 25.0.0

#### Build / Linux

- dcfa002debee7a2203f94c633021bf7f13fb6bfa Fixes 445 - use \*scanf format specifier

#### Build / Windows

None.

#### Build / MacOSX

None.

#### Tests / test framework

- 491ff2fc367affa6dbefeb7528d4268b38630066 [test] Added verbose printing if script_flags fuzzer fails
- 1f11ca804f481adbb7f14c8d6a3c0542d4ccf201 [test] Add test harness for UI tests
- d624f87a832415dbc54055d594c696deaacc59db test: Count blocks and headers consistently in case of reorg
- 4f2851cf832e0673bc81893d24150c21307a27e4 [test] Fix fuzz test for script_flags
- e61dc9ff3aec4abd2664b7402800bc3efbc1351b [test] Integrate BCHN issue #440 regression test into rpc_psbt.py
- 61edcc8808dd0946dc6ed8fcded3062f8fec99c3 [test] Revert RSS limit change in fuzz test runner
- 91f5e93142aa1ef45c284a8e30171546b3c4a8b9 [test] Tune libFuzzer parameters passed by fuzz test runner
- d28be6e06da8f31c9fd3a9dc15e430e2494da8c6 Remove "replay protection" column from sighash json test data
- cb23157c292cfcedc09cb6d199db3bfbe311b087 tests: Add RPC-based basic tests to send/receive tokens to/from wallet

#### Benchmarks

- 452b93603d73035e50ca8d666fe13524e4deab71 Benchmarks: Independent runs for each eval

#### Seeds / seeder software

- 93afd02e8e0bc9bcf867d749bed6db86e4449910 Update seeds for mainnet and testnet4, plus fix bug in generare-seeds.py
- c0fc6d42df6edba9aa938c336269eaeef86e29a8 Update `contrib/seeds/makseeds.py` regular expression

#### Maintainer tools

- 865f9fa25cdd68b0c71225fc77433447c324ec64 [qa] Benchmark bisect script, and bench script enhancements
- b7a72f6bac2dc5ada399d36bb57626d85218d425 [qa] Contrib script to accurately update copyright headers
- 281d39ce803813fd3f3f17407d08affa33c308b0 lint: Allow std::variant to appear in codebase.
- da759a17a9a3c65bf24d81cc662046ae2d89d2b9 Fix bug in make_chainparams.py which confuses ScaleNet with ChipNet
- 35a9c1e07a97dccbb8f463abb6ebb346e1cecae1 Add linting support for raw string literals
- 0ca253393d5d81a2a74a4c5736043d4595d28a5b Copyright header script fixes

#### Infrastructure

- 652f773ec91560e4cebbc641e3619ec17affd2c7 [qa] Update blockchain data sizes for mainnet for v25.0.0
- 66fd14b3414a021a45bf4d5acd6e904a2782c554 Update chainTxData for main, test3, test4, scalenet, and chipnet

#### Cleanup

None.

#### Continuous Integration (GitLab CI)

- fdc3c1c644aaeef4054423cf550286b3e2f4dd5f [ci] Hotfix for 'pages' job: pin mkdocs-material to 8.2.16
- 097502cf5883400fc2e6a19987cfe59917264c57 [ci] Unpin mkdocs-material dependency after upstream fix

#### Backports

- c0178ff3a433b13f18cba6dd420ae8c3570dd68f [backport#14689] Require a public key to be retrieved when signing a P2PKH input
- a74e330344bdedcc626244c0d6f168857a777941 [backport#14690] Throw error if CPubKey is invalid during PSBT keypath serialization
- 7512171d50fca80f1b37f0528081b4d00df7d305 [backport] Add pure Python RIPEMD-160
- 47a6e9cfed2575baf4852b9af1832ba698a0b5fb [backport] fix assert crash when specified change output spend size is unknown
- 25d99c1b34debe94ae435e9d2c1aec8c5136d58d [backport] Replace MakeSpan helper with Span deduction guide
- 27f97a8a82f26d99489c69b41d14aa5bffd2c3f6 [backport] secp256k1: Remove OpenSSL testing support
- 141afbe3170884a8322ab7d4db57286839affeab [backport] Swap out hashlib.ripemd160 for own implementation
- 2746e656120f85026b02eef2c6f1af6a409c81f8 Walk pindexBestHeader back to ChainActive().Tip() if it is invalid
