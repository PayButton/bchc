// Copyright (c) 2017 The Bitcoin Core developers
// Copyright (c) 2019-2024 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <core_io.h>
#include <key.h>
#include <keystore.h>
#include <policy/policy.h>
#include <script/ismine.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/standard.h>
#include <test/setup_common.h>
#include <tinyformat.h>

#include <boost/test/unit_test.hpp>

// Append given push onto a script, using specific opcode (not necessarily
// the minimal one, but must be able to contain the given data.)
void AppendPush(CScript &script, opcodetype opcode,
                const std::vector<uint8_t> &b) {
    assert(opcode <= OP_PUSHDATA4);
    script.push_back(opcode);
    if (opcode < OP_PUSHDATA1) {
        assert(b.size() == opcode);
    } else if (opcode == OP_PUSHDATA1) {
        assert(b.size() <= 0xff);
        script.push_back(uint8_t(b.size()));
    } else if (opcode == OP_PUSHDATA2) {
        assert(b.size() <= 0xffff);
        uint8_t _data[2];
        WriteLE16(_data, b.size());
        script.insert(script.end(), _data, _data + sizeof(_data));
    } else if (opcode == OP_PUSHDATA4) {
        uint8_t _data[4];
        WriteLE32(_data, b.size());
        script.insert(script.end(), _data, _data + sizeof(_data));
    }
    script.insert(script.end(), b.begin(), b.end());
}

BOOST_FIXTURE_TEST_SUITE(script_standard_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(script_standard_Solver_success) {
    for (const bool is_p2sh_32 : {false, true}) {
        const uint32_t flags = is_p2sh_32 ? STANDARD_SCRIPT_VERIFY_FLAGS | SCRIPT_ENABLE_P2SH_32
                                          : STANDARD_SCRIPT_VERIFY_FLAGS & ~SCRIPT_ENABLE_P2SH_32;

        CKey keys[3];
        CPubKey pubkeys[3];
        for (int i = 0; i < 3; i++) {
            keys[i].MakeNewKey(true);
            pubkeys[i] = keys[i].GetPubKey();
        }

        CScript s;
        std::vector<std::vector<uint8_t>> solutions;

        // TX_PUBKEY
        s.clear();
        s << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
        BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_PUBKEY);
        BOOST_CHECK_EQUAL(solutions.size(), 1U);
        BOOST_CHECK(solutions[0] == ToByteVector(pubkeys[0]));

        // TX_PUBKEYHASH
        s.clear();
        s << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID())
          << OP_EQUALVERIFY << OP_CHECKSIG;
        BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_PUBKEYHASH);
        BOOST_CHECK_EQUAL(solutions.size(), 1U);
        BOOST_CHECK(solutions[0] == ToByteVector(pubkeys[0].GetID()));

        // TX_SCRIPTHASH
        const CScript redeemScript(s); // initialize with leftover P2PKH script
        s.clear();
        s << OP_HASH160 << ToByteVector(ScriptID(redeemScript, false /* p2sh_20 */)) << OP_EQUAL;
        BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_SCRIPTHASH);
        BOOST_CHECK_EQUAL(solutions.size(), 1U);
        BOOST_CHECK(solutions[0] == ToByteVector(ScriptID(redeemScript, false /* p2sh_20 */)));

        // TX_SCRIPTHASH (P2SH_32)
        s.clear();
        s << OP_HASH256 << ToByteVector(ScriptID(redeemScript, true /* p2sh_32 */)) << OP_EQUAL;
        if (is_p2sh_32) {
            // If we are looping and p2sh_32 is enabled, we expect this
            BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_SCRIPTHASH);
            BOOST_CHECK_EQUAL(solutions.size(), 1U);
            BOOST_CHECK(solutions[0] == ToByteVector(ScriptID(redeemScript, true /* p2sh_32 */)));
        } else {
            // Otherwise we expect this
            BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
            BOOST_CHECK_EQUAL(solutions.size(), 0U);
        }

        // TX_MULTISIG
        s.clear();
        s << OP_1 << ToByteVector(pubkeys[0]) << ToByteVector(pubkeys[1]) << OP_2
          << OP_CHECKMULTISIG;
        BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_MULTISIG);
        BOOST_CHECK_EQUAL(solutions.size(), 4U);
        BOOST_CHECK(solutions[0] == std::vector<uint8_t>({1}));
        BOOST_CHECK(solutions[1] == ToByteVector(pubkeys[0]));
        BOOST_CHECK(solutions[2] == ToByteVector(pubkeys[1]));
        BOOST_CHECK(solutions[3] == std::vector<uint8_t>({2}));

        s.clear();
        s << OP_2 << ToByteVector(pubkeys[0]) << ToByteVector(pubkeys[1])
          << ToByteVector(pubkeys[2]) << OP_3 << OP_CHECKMULTISIG;
        BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_MULTISIG);
        BOOST_CHECK_EQUAL(solutions.size(), 5U);
        BOOST_CHECK(solutions[0] == std::vector<uint8_t>({2}));
        BOOST_CHECK(solutions[1] == ToByteVector(pubkeys[0]));
        BOOST_CHECK(solutions[2] == ToByteVector(pubkeys[1]));
        BOOST_CHECK(solutions[3] == ToByteVector(pubkeys[2]));
        BOOST_CHECK(solutions[4] == std::vector<uint8_t>({3}));

        // TX_NULL_DATA
        s.clear();
        s << OP_RETURN << std::vector<uint8_t>({0}) << std::vector<uint8_t>({75})
          << std::vector<uint8_t>({255});
        BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NULL_DATA);
        BOOST_CHECK_EQUAL(solutions.size(), 0U);

        // TX_WITNESS_V0_KEYHASH
        s.clear();
        s << OP_0 << ToByteVector(pubkeys[0].GetID());
        BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
        BOOST_CHECK_EQUAL(solutions.size(), 0U);

        // TX_WITNESS_V0_SCRIPTHASH
        uint256 scriptHash;
        CSHA256()
            .Write(&redeemScript[0], redeemScript.size())
            .Finalize(scriptHash.begin());

        s.clear();
        s << OP_0 << ToByteVector(scriptHash);
        BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
        BOOST_CHECK_EQUAL(solutions.size(), 0U);

        // TX_NONSTANDARD
        s.clear();
        s << OP_9 << OP_ADD << OP_11 << OP_EQUAL;
        BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
        BOOST_CHECK_EQUAL(solutions.size(), 0);

        // Try some non-minimal PUSHDATA pushes in various standard scripts
        for (auto pushdataop : {OP_PUSHDATA1, OP_PUSHDATA2, OP_PUSHDATA4}) {
            // mutated TX_PUBKEY
            s.clear();
            AppendPush(s, pushdataop, ToByteVector(pubkeys[0]));
            s << OP_CHECKSIG;
            BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
            BOOST_CHECK_EQUAL(solutions.size(), 0);

            // mutated TX_PUBKEYHASH
            s.clear();
            s << OP_DUP << OP_HASH160;
            AppendPush(s, pushdataop, ToByteVector(pubkeys[0].GetID()));
            s << OP_EQUALVERIFY << OP_CHECKSIG;
            BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
            BOOST_CHECK_EQUAL(solutions.size(), 0);

            // mutated TX_SCRIPTHASH
            s.clear();
            s << OP_HASH160;
            AppendPush(s, pushdataop, ToByteVector(ScriptID(redeemScript, false /* p2sh_20 */)));
            s << OP_EQUAL;
            BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
            BOOST_CHECK_EQUAL(solutions.size(), 0);

            // mutated TX_SCRIPTHASH (P2SH_32)
            s.clear();
            s << OP_HASH256;
            AppendPush(s, pushdataop, ToByteVector(ScriptID(redeemScript, true /* p2sh_32 */)));
            s << OP_EQUAL;
            BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
            BOOST_CHECK_EQUAL(solutions.size(), 0);

            // mutated TX_MULTISIG -- pubkey
            s.clear();
            s << OP_1;
            AppendPush(s, pushdataop, ToByteVector(pubkeys[0]));
            s << ToByteVector(pubkeys[1]) << OP_2 << OP_CHECKMULTISIG;
            BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
            BOOST_CHECK_EQUAL(solutions.size(), 0);

            // mutated TX_MULTISIG -- num_signatures
            s.clear();
            AppendPush(s, pushdataop, {1});
            s << ToByteVector(pubkeys[0]) << ToByteVector(pubkeys[1]) << OP_2
              << OP_CHECKMULTISIG;
            BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
            BOOST_CHECK_EQUAL(solutions.size(), 0);

            // mutated TX_MULTISIG -- num_pubkeys
            s.clear();
            s << OP_1 << ToByteVector(pubkeys[0]) << ToByteVector(pubkeys[1]);
            AppendPush(s, pushdataop, {2});
            s << OP_CHECKMULTISIG;
            BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
            BOOST_CHECK_EQUAL(solutions.size(), 0);
        }

        // also try pushing the num_signatures and num_pubkeys using PUSH_N opcode
        // instead of OP_N opcode:
        s.clear();
        s << std::vector<uint8_t>{1} << ToByteVector(pubkeys[0])
          << ToByteVector(pubkeys[1]) << OP_2 << OP_CHECKMULTISIG;
        BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
        BOOST_CHECK_EQUAL(solutions.size(), 0);
        s.clear();
        s << OP_1 << ToByteVector(pubkeys[0]) << ToByteVector(pubkeys[1])
          << std::vector<uint8_t>{2} << OP_CHECKMULTISIG;
        BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
        BOOST_CHECK_EQUAL(solutions.size(), 0);

        // Non-minimal pushes in OP_RETURN scripts are standard (some OP_RETURN
        // protocols like SLP rely on this). Also it turns out OP_RESERVED gets past
        // IsPushOnly and thus is standard here.
        std::vector<uint8_t> op_return_nonminimal{
            OP_RETURN,    OP_RESERVED, OP_PUSHDATA1, 0x00, 0x01, 0x01,
            OP_PUSHDATA4, 0x01,        0x00,         0x00, 0x00, 0xaa};
        s.assign(op_return_nonminimal.begin(), op_return_nonminimal.end());
        BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NULL_DATA);
        BOOST_CHECK_EQUAL(solutions.size(), 0);
    }
}

BOOST_AUTO_TEST_CASE(script_standard_Solver_failure) {
    const uint32_t flags = STANDARD_SCRIPT_VERIFY_FLAGS | SCRIPT_ENABLE_P2SH_32;

    CKey key;
    CPubKey pubkey;
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    CScript s;
    std::vector<std::vector<uint8_t>> solutions;

    // TX_PUBKEY with incorrectly sized pubkey
    s.clear();
    s << std::vector<uint8_t>(30, 0x01) << OP_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);

    // TX_PUBKEYHASH with incorrectly sized key hash
    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkey) << OP_EQUALVERIFY
      << OP_CHECKSIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);

    // TX_SCRIPTHASH with incorrectly sized script hash
    s.clear();
    s << OP_HASH160 << std::vector<uint8_t>(21, 0x01) << OP_EQUAL;
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);

    // TX_SCRIPTHASH P2SH_32 with incorrectly sized script hash
    s.clear();
    s << OP_HASH256 << std::vector<uint8_t>(33, 0x01) << OP_EQUAL;
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);

    // TX_SCRIPTHASH P2SH_32 with SCRIPT_ENABLE_P2SH_32 disabled
    s.clear();
    s << OP_HASH256 << std::vector<uint8_t>(32, 0x01) << OP_EQUAL;
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags | SCRIPT_ENABLE_P2SH_32), TX_SCRIPTHASH);
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags & ~SCRIPT_ENABLE_P2SH_32), TX_NONSTANDARD);

    // TX_MULTISIG 0/2
    s.clear();
    s << OP_0 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);

    // TX_MULTISIG 2/1
    s.clear();
    s << OP_2 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);

    // TX_MULTISIG n = 2 with 1 pubkey
    s.clear();
    s << OP_1 << ToByteVector(pubkey) << OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);

    // TX_MULTISIG n = 1 with 0 pubkeys
    s.clear();
    s << OP_1 << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);

    // TX_NULL_DATA with other opcodes
    s.clear();
    s << OP_RETURN << std::vector<uint8_t>({75}) << OP_ADD;
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);

    // TX_WITNESS with unknown version
    s.clear();
    s << OP_1 << ToByteVector(pubkey);
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);

    // TX_WITNESS with incorrect program size
    s.clear();
    s << OP_0 << std::vector<uint8_t>(19, 0x01);
    BOOST_CHECK_EQUAL(Solver(s, solutions, flags), TX_NONSTANDARD);
}

BOOST_AUTO_TEST_CASE(script_standard_ExtractDestination) {
    const uint32_t flags = STANDARD_SCRIPT_VERIFY_FLAGS | SCRIPT_ENABLE_P2SH_32;

    CKey key;
    CPubKey pubkey;
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    CScript s;
    CTxDestination address;

    // TX_PUBKEY
    s.clear();
    s << ToByteVector(pubkey) << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestination(s, address, flags));
    BOOST_CHECK(boost::get<CKeyID>(&address) &&
                *boost::get<CKeyID>(&address) == pubkey.GetID());

    // TX_PUBKEYHASH
    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkey.GetID()) << OP_EQUALVERIFY
      << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestination(s, address, flags));
    BOOST_CHECK(boost::get<CKeyID>(&address) &&
                *boost::get<CKeyID>(&address) == pubkey.GetID());

    // TX_SCRIPTHASH
    const CScript redeemScript(s); // initialize with leftover P2PKH script
    s.clear();
    s << OP_HASH160 << ToByteVector(ScriptID(redeemScript, false /* p2sh_20 */)) << OP_EQUAL;
    BOOST_CHECK(ExtractDestination(s, address, flags));
    BOOST_CHECK(boost::get<ScriptID>(&address) &&
                *boost::get<ScriptID>(&address) == ScriptID(redeemScript, false /* p2sh_20 */));

    // TX_SCRIPTHASH (P2SH_32)
    s.clear();
    s << OP_HASH256 << ToByteVector(ScriptID(redeemScript, true /* p2sh_32 */)) << OP_EQUAL;
    BOOST_CHECK(ExtractDestination(s, address, flags));
    BOOST_CHECK(boost::get<ScriptID>(&address) &&
                *boost::get<ScriptID>(&address) == ScriptID(redeemScript, true /* p2sh_32 */));
    BOOST_CHECK_MESSAGE(!ExtractDestination(s, address, flags & ~SCRIPT_ENABLE_P2SH_32),
                        strprintf("When disabling SCRIPT_ENABLE_P2SH_32, expected ExtractDestination to fail: %s",
                                  ScriptToAsmStr(s)));

    // TX_MULTISIG
    s.clear();
    s << OP_1 << ToByteVector(pubkey) << OP_1 << OP_CHECKMULTISIG;
    BOOST_CHECK(!ExtractDestination(s, address, flags));

    // TX_NULL_DATA
    s.clear();
    s << OP_RETURN << std::vector<uint8_t>({75});
    BOOST_CHECK(!ExtractDestination(s, address, flags));

    // TX_WITNESS_V0_KEYHASH
    s.clear();
    s << OP_0 << ToByteVector(pubkey);
    BOOST_CHECK(!ExtractDestination(s, address, flags));

    // TX_WITNESS_V0_SCRIPTHASH
    s.clear();
    s << OP_0 << ToByteVector(ScriptID(redeemScript, false /* p2sh_20 */));
    BOOST_CHECK(!ExtractDestination(s, address, flags));

    // TX_WITNESS_V0_SCRIPTHASH (P2SH32; nonsensical)
    s.clear();
    s << OP_0 << ToByteVector(ScriptID(redeemScript, true /* p2sh_32 */));
    BOOST_CHECK(!ExtractDestination(s, address, flags));
}

BOOST_AUTO_TEST_CASE(script_standard_ExtractDestinations) {
    const uint32_t flags = STANDARD_SCRIPT_VERIFY_FLAGS | SCRIPT_ENABLE_P2SH_32;

    CKey keys[3];
    CPubKey pubkeys[3];
    for (int i = 0; i < 3; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CScript s;
    txnouttype whichType;
    std::vector<CTxDestination> addresses;
    int nRequired;

    // TX_PUBKEY
    s.clear();
    s << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired, flags));
    BOOST_CHECK_EQUAL(whichType, TX_PUBKEY);
    BOOST_CHECK_EQUAL(addresses.size(), 1U);
    BOOST_CHECK_EQUAL(nRequired, 1);
    BOOST_CHECK(boost::get<CKeyID>(&addresses[0]) &&
                *boost::get<CKeyID>(&addresses[0]) == pubkeys[0].GetID());

    // TX_PUBKEYHASH
    s.clear();
    s << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID())
      << OP_EQUALVERIFY << OP_CHECKSIG;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired, flags));
    BOOST_CHECK_EQUAL(whichType, TX_PUBKEYHASH);
    BOOST_CHECK_EQUAL(addresses.size(), 1U);
    BOOST_CHECK_EQUAL(nRequired, 1);
    BOOST_CHECK(boost::get<CKeyID>(&addresses[0]) &&
                *boost::get<CKeyID>(&addresses[0]) == pubkeys[0].GetID());

    // TX_SCRIPTHASH
    // initialize with leftover P2PKH script
    const CScript redeemScript(s);
    s.clear();
    s << OP_HASH160 << ToByteVector(ScriptID(redeemScript, false /* p2sh_20 */)) << OP_EQUAL;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired, flags));
    BOOST_CHECK_EQUAL(whichType, TX_SCRIPTHASH);
    BOOST_CHECK_EQUAL(addresses.size(), 1U);
    BOOST_CHECK_EQUAL(nRequired, 1);
    BOOST_CHECK(boost::get<ScriptID>(&addresses[0]) &&
                *boost::get<ScriptID>(&addresses[0]) ==
                    ScriptID(redeemScript, false /* p2sh_20 */));

    // TX_SCRIPTHASH (P2SH_32)
    // initialize with leftover P2PKH script
    s.clear();
    s << OP_HASH256 << ToByteVector(ScriptID(redeemScript, true /* p2sh_32 */)) << OP_EQUAL;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired, flags));
    BOOST_CHECK_EQUAL(whichType, TX_SCRIPTHASH);
    BOOST_CHECK_EQUAL(addresses.size(), 1U);
    BOOST_CHECK_EQUAL(nRequired, 1);
    BOOST_CHECK(boost::get<ScriptID>(&addresses[0]) &&
                *boost::get<ScriptID>(&addresses[0]) == ScriptID(redeemScript, true /* p2sh_32 */));

    // TX_MULTISIG
    s.clear();
    s << OP_2 << ToByteVector(pubkeys[0]) << ToByteVector(pubkeys[1]) << OP_2
      << OP_CHECKMULTISIG;
    BOOST_CHECK(ExtractDestinations(s, whichType, addresses, nRequired, flags));
    BOOST_CHECK_EQUAL(whichType, TX_MULTISIG);
    BOOST_CHECK_EQUAL(addresses.size(), 2U);
    BOOST_CHECK_EQUAL(nRequired, 2);
    BOOST_CHECK(boost::get<CKeyID>(&addresses[0]) &&
                *boost::get<CKeyID>(&addresses[0]) == pubkeys[0].GetID());
    BOOST_CHECK(boost::get<CKeyID>(&addresses[1]) &&
                *boost::get<CKeyID>(&addresses[1]) == pubkeys[1].GetID());

    // TX_NULL_DATA
    s.clear();
    s << OP_RETURN << std::vector<uint8_t>({75});
    BOOST_CHECK(!ExtractDestinations(s, whichType, addresses, nRequired, flags));

    // TX_WITNESS_V0_KEYHASH
    s.clear();
    s << OP_0 << ToByteVector(pubkeys[0].GetID());
    BOOST_CHECK(!ExtractDestinations(s, whichType, addresses, nRequired, flags));

    // TX_WITNESS_V0_SCRIPTHASH
    s.clear();
    s << OP_0 << ToByteVector(ScriptID(redeemScript, false /* p2sh_20 */));
    BOOST_CHECK(!ExtractDestinations(s, whichType, addresses, nRequired, flags));

    // TX_WITNESS_V0_SCRIPTHASH using p2sh_32 (nonsenical)
    s.clear();
    s << OP_0 << ToByteVector(ScriptID(redeemScript, true /* p2sh_32 */));
    BOOST_CHECK(!ExtractDestinations(s, whichType, addresses, nRequired, flags));
}

BOOST_AUTO_TEST_CASE(script_standard_GetScriptFor_) {
    CKey keys[3];
    CPubKey pubkeys[3];
    for (int i = 0; i < 3; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CScript expected, result;

    // CKeyID
    expected.clear();
    expected << OP_DUP << OP_HASH160 << ToByteVector(pubkeys[0].GetID())
             << OP_EQUALVERIFY << OP_CHECKSIG;
    result = GetScriptForDestination(pubkeys[0].GetID());
    BOOST_CHECK(result == expected);

    // ScriptID - p2sh_20 (legacy)
    CScript redeemScript(result);
    expected.clear();
    expected << OP_HASH160 << ToByteVector(ScriptID(redeemScript, false)) << OP_EQUAL;
    result = GetScriptForDestination(ScriptID(redeemScript, false));
    BOOST_CHECK(result == expected);

    // ScriptID - p2sh_32 (newer)
    expected.clear();
    expected << OP_HASH256 << ToByteVector(ScriptID(redeemScript, true /* p2sh_32 */)) << OP_EQUAL;
    result = GetScriptForDestination(ScriptID(redeemScript, true /* p2sh_32 */));
    BOOST_CHECK(result == expected);

    // CNoDestination
    expected.clear();
    result = GetScriptForDestination(CNoDestination());
    BOOST_CHECK(result == expected);

    // GetScriptForRawPubKey
    expected.clear();
    expected << ToByteVector(pubkeys[0]) << OP_CHECKSIG;
    result = GetScriptForRawPubKey(pubkeys[0]);
    BOOST_CHECK(result == expected);

    // GetScriptForMultisig
    expected.clear();
    expected << OP_2 << ToByteVector(pubkeys[0]) << ToByteVector(pubkeys[1])
             << ToByteVector(pubkeys[2]) << OP_3 << OP_CHECKMULTISIG;
    result =
        GetScriptForMultisig(2, std::vector<CPubKey>(pubkeys, pubkeys + 3));
    BOOST_CHECK(result == expected);
}

BOOST_AUTO_TEST_CASE(script_standard_IsMine) {
    CKey keys[2];
    CPubKey pubkeys[2];
    for (int i = 0; i < 2; i++) {
        keys[i].MakeNewKey(true);
        pubkeys[i] = keys[i].GetPubKey();
    }

    CKey uncompressedKey;
    uncompressedKey.MakeNewKey(false);
    CPubKey uncompressedPubkey = uncompressedKey.GetPubKey();

    CScript scriptPubKey;
    isminetype result;

    // P2PK compressed
    {
        CBasicKeyStore keystore;
        scriptPubKey = GetScriptForRawPubKey(pubkeys[0]);

        // Keystore does not have key
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

        // Keystore has key
        BOOST_CHECK(keystore.AddKey(keys[0]));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

    // P2PK uncompressed
    {
        CBasicKeyStore keystore;
        scriptPubKey = GetScriptForRawPubKey(uncompressedPubkey);

        // Keystore does not have key
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

        // Keystore has key
        BOOST_CHECK(keystore.AddKey(uncompressedKey));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

    // P2PKH compressed
    {
        CBasicKeyStore keystore;
        scriptPubKey = GetScriptForDestination(pubkeys[0].GetID());

        // Keystore does not have key
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

        // Keystore has key
        BOOST_CHECK(keystore.AddKey(keys[0]));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

    // P2PKH uncompressed
    {
        CBasicKeyStore keystore;
        scriptPubKey = GetScriptForDestination(uncompressedPubkey.GetID());

        // Keystore does not have key
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

        // Keystore has key
        BOOST_CHECK(keystore.AddKey(uncompressedKey));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
    }

    // P2SH-20
    {
        CBasicKeyStore keystore;

        CScript redeemScript = GetScriptForDestination(pubkeys[0].GetID());
        scriptPubKey = GetScriptForDestination(ScriptID(redeemScript, false /*=p2sh_20*/));

        // Keystore does not have redeemScript or key
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

        // Keystore has redeemScript but no key
        BOOST_CHECK(keystore.AddCScript(redeemScript, false /*=p2sh_20*/, false /* legacy vm limits */));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

        // Keystore has redeemScript and key
        BOOST_CHECK(keystore.AddKey(keys[0]));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);

        // Ensure that if we only added the P2SH-20, we don't also match P2SH-32
        CScript scriptPubKey32 = GetScriptForDestination(ScriptID(redeemScript, true /*=p2sh_32*/));
        result = IsMine(keystore, scriptPubKey32);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

    // P2SH-32
    {
        CBasicKeyStore keystore;

        CScript redeemScript = GetScriptForDestination(pubkeys[0].GetID());
        scriptPubKey = GetScriptForDestination(ScriptID(redeemScript, true /*=p2sh_32*/));

        // Keystore does not have redeemScript or key
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

        // Keystore has redeemScript but no key
        BOOST_CHECK(keystore.AddCScript(redeemScript, true /*=p2sh_32*/, false /* legacy vm limits */));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

        // Keystore has redeemScript and key
        BOOST_CHECK(keystore.AddKey(keys[0]));
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);

        // Ensure that if we only added the P2SH-32, we don't also match P2SH-20
        CScript scriptPubKey20 = GetScriptForDestination(ScriptID(redeemScript, false /*=p2sh_20*/));
        result = IsMine(keystore, scriptPubKey20);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

    // (P2PKH inside) P2SH inside P2SH (invalid)
    {
        for (const bool is_p2sh_32 : {false, true}) {
            CBasicKeyStore keystore;

            CScript redeemscript_inner = GetScriptForDestination(pubkeys[0].GetID());
            CScript redeemscript = GetScriptForDestination(ScriptID(redeemscript_inner, is_p2sh_32));
            scriptPubKey = GetScriptForDestination(ScriptID(redeemscript, is_p2sh_32));

            BOOST_CHECK(keystore.AddCScript(redeemscript, is_p2sh_32, false /* legacy vm limits */));
            BOOST_CHECK(keystore.AddCScript(redeemscript_inner, is_p2sh_32, false /* legacy vm limits */));
            BOOST_CHECK(keystore.AddCScript(scriptPubKey, is_p2sh_32, false /* legacy vm limits */));
            BOOST_CHECK(keystore.AddKey(keys[0]));
            result = IsMine(keystore, scriptPubKey);
            BOOST_CHECK_EQUAL(result, ISMINE_NO);
        }
    }

    // scriptPubKey multisig
    {
        CBasicKeyStore keystore;

        scriptPubKey =
            GetScriptForMultisig(2, {uncompressedPubkey, pubkeys[1]});

        // Keystore does not have any keys
        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

        // Keystore has 1/2 keys
        BOOST_CHECK(keystore.AddKey(uncompressedKey));

        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

        // Keystore has 2/2 keys
        BOOST_CHECK(keystore.AddKey(keys[1]));

        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);

        // Keystore has 2/2 keys and the script
        BOOST_CHECK(keystore.AddCScript(scriptPubKey, false /*=p2sh_20*/, false /* legacy vm limits */));

        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

    // P2SH multisig
    {
        for (const bool is_p2sh_32 : {false, true}) {
            CBasicKeyStore keystore;
            BOOST_CHECK(keystore.AddKey(uncompressedKey));
            BOOST_CHECK(keystore.AddKey(keys[1]));

            CScript redeemScript = GetScriptForMultisig(2, {uncompressedPubkey, pubkeys[1]});
            scriptPubKey = GetScriptForDestination(ScriptID(redeemScript, is_p2sh_32));

            // Keystore has no redeemScript
            result = IsMine(keystore, scriptPubKey);
            BOOST_CHECK_EQUAL(result, ISMINE_NO);

            // Keystore has redeemScript
            BOOST_CHECK(keystore.AddCScript(redeemScript, is_p2sh_32, false /* legacy vm limits */));
            result = IsMine(keystore, scriptPubKey);
            BOOST_CHECK_EQUAL(result, ISMINE_SPENDABLE);
        }
    }

    // OP_RETURN
    {
        CBasicKeyStore keystore;
        BOOST_CHECK(keystore.AddKey(keys[0]));

        scriptPubKey.clear();
        scriptPubKey << OP_RETURN << ToByteVector(pubkeys[0]);

        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }

    // Nonstandard
    {
        CBasicKeyStore keystore;
        BOOST_CHECK(keystore.AddKey(keys[0]));

        scriptPubKey.clear();
        scriptPubKey << OP_9 << OP_ADD << OP_11 << OP_EQUAL;

        result = IsMine(keystore, scriptPubKey);
        BOOST_CHECK_EQUAL(result, ISMINE_NO);
    }
}

BOOST_AUTO_TEST_SUITE_END()
