// Copyright (c) 2018-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <core_io.h>

#include <script/script_flags.h>
#include <script/sighashtype.h>
#include <test/setup_common.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <cassert>
#include <iomanip>
#include <map>
#include <string>

BOOST_FIXTURE_TEST_SUITE(core_io_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(parse_hex_test) {
    std::string s = "0x";
    BOOST_CHECK_THROW(ParseScript(s), std::runtime_error);

    for (int numZeroes = 1; numZeroes <= 32; numZeroes++) {
        s += "0";
        if (numZeroes % 2 == 0) {
            BOOST_CHECK_NO_THROW(ParseScript(s));
        } else {
            BOOST_CHECK_THROW(ParseScript(s), std::runtime_error);
        }
    }
}

static void PrintLE(std::ostringstream &testString, size_t bytes, size_t pushLength) {
    testString << "0x";
    while (bytes != 0) {
        testString << std::setfill('0') << std::setw(2) << std::hex << pushLength % 256;
        pushLength /= 256;
        bytes--;
    }
}

static std::string TestPushOpcode(size_t pushWidth, size_t pushLength, size_t actualLength) {
    std::ostringstream testString;

    switch (pushWidth) {
        case 1:
            testString << "PUSHDATA1 ";
            break;
        case 2:
            testString << "PUSHDATA2 ";
            break;
        case 4:
            testString << "PUSHDATA4 ";
            break;
        default:
            assert(false);
    }
    PrintLE(testString, pushWidth, pushLength);
    testString << " 0x";

    for (size_t i = 0; i < actualLength; i++) {
        testString << "01";
    }

    return testString.str();
}

BOOST_AUTO_TEST_CASE(printle_tests) {
    // Ensure the test generator is doing what we think it is.
    std::ostringstream testString;
    PrintLE(testString, 04, 0x8001);
    BOOST_CHECK_EQUAL(testString.str(), "0x01800000");
}

BOOST_AUTO_TEST_CASE(testpushopcode_tests) {
    BOOST_CHECK_EQUAL(TestPushOpcode(1, 2, 2), "PUSHDATA1 0x02 0x0101");
    BOOST_CHECK_EQUAL(TestPushOpcode(2, 2, 2), "PUSHDATA2 0x0200 0x0101");
    BOOST_CHECK_EQUAL(TestPushOpcode(4, 2, 2), "PUSHDATA4 0x02000000 0x0101");
}

BOOST_AUTO_TEST_CASE(parse_push_test) {
    BOOST_CHECK_NO_THROW(ParseScript("0x01 0x01"));
    BOOST_CHECK_NO_THROW(ParseScript("0x01 XOR"));
    BOOST_CHECK_NO_THROW(ParseScript("0x01 1"));
    BOOST_CHECK_NO_THROW(ParseScript("0x01 ''"));
    BOOST_CHECK_NO_THROW(ParseScript("0x02 0x0101"));
    BOOST_CHECK_NO_THROW(ParseScript("0x02 42"));
    BOOST_CHECK_NO_THROW(ParseScript("0x02 'a'"));

    BOOST_CHECK_THROW(ParseScript("0x01 0x0101"), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript("0x01 42"), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript("0x02 0x01"), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript("0x02 XOR"), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript("0x02 1"), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript("0x02 ''"), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript("0x02 0x010101"), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript("0x02 'ab'"), std::runtime_error);

    // Note sizes are LE encoded.  Also, some of these values are not
    // minimally encoded intentionally -- nor are they being required to be
    // minimally encoded.
    BOOST_CHECK_NO_THROW(ParseScript("PUSHDATA4 0x02000000 0x0101"));
    BOOST_CHECK_THROW(ParseScript("PUSHDATA4 0x03000000 0x0101"), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript("PUSHDATA4 0x02000000 0x010101"), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript("PUSHDATA4 0x020000 0x0101"), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript("PUSHDATA4 0x0200000000 0x0101"), std::runtime_error);

    BOOST_CHECK_NO_THROW(ParseScript("PUSHDATA2 0x0200 0x0101"));
    BOOST_CHECK_THROW(ParseScript("PUSHDATA2 0x0300 0x0101"), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript("PUSHDATA2 0x030000 0x0101"), std::runtime_error);
    BOOST_CHECK_NO_THROW(ParseScript("PUSHDATA1 0x02 0x0101"));
    BOOST_CHECK_THROW(ParseScript("PUSHDATA1 0x02 0x010101"), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript("PUSHDATA1 0x0200 0x010101"), std::runtime_error);

    // Ensure pushdata handling is not using 1's complement
    BOOST_CHECK_NO_THROW(ParseScript(TestPushOpcode(1, 0xC8, 0xC8)));
    BOOST_CHECK_THROW(ParseScript(TestPushOpcode(1, 0xC8, 0xC9)), std::runtime_error);

    BOOST_CHECK_NO_THROW(ParseScript(TestPushOpcode(2, 0x8000, 0x8000)));
    BOOST_CHECK_THROW(ParseScript(TestPushOpcode(2, 0x8000, 0x8001)), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript(TestPushOpcode(2, 0x8001, 0x8000)), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript(TestPushOpcode(2, 0x80, 0x81)), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript(TestPushOpcode(2, 0x80, 0x7F)), std::runtime_error);

    // Can't build something too long.
    BOOST_CHECK_NO_THROW(ParseScript(TestPushOpcode(4, 0x8000, 0x8000)));
    BOOST_CHECK_THROW(ParseScript(TestPushOpcode(4, 0x8000, 0x8001)), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript(TestPushOpcode(4, 0x8001, 0x8000)), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript(TestPushOpcode(4, 0x80, 0x81)), std::runtime_error);
    BOOST_CHECK_THROW(ParseScript(TestPushOpcode(4, 0x80, 0x7F)), std::runtime_error);
}

void TestFormatRoundTrip(const std::string &script) {
    BOOST_CHECK_EQUAL(script, FormatScript(ParseScript(script)));
}

BOOST_AUTO_TEST_CASE(format_script_test) {
    TestFormatRoundTrip("0 1 5 CHECKDATASIG CHECKSIG XOR NOP5 NOP10 "
                        "CHECKDATASIGVERIFY DEPTH RETURN VERIFY SPLIT INVERT "
                        "EQUAL HASH256 GREATERTHANOREQUAL RSHIFT");
}

BOOST_AUTO_TEST_CASE(parse_hash_str) {
    { //uint160
        uint160 expected, parsed;
        const uint8_t expectedB[] = {11, 7, 174, 137, 172, 8, 44, 53, 28, 68, 166, 150, 72, 157, 105, 93, 215, 100, 211, 80};
        std::memcpy(expected.begin(), expectedB, std::min(size_t(expected.size()), sizeof(expectedB)));
        BOOST_CHECK_MESSAGE(ParseHashStr("50d364d75d699d4896a6441c352c08ac89ae070b", parsed) && parsed == expected,
                            "Parsing hash160 should yield the expected result");
    }
    { //uint256
        uint256 expected, parsed;
        const uint8_t expectedB[] = {254, 163, 200, 88, 44, 199, 20, 58, 138, 71, 160, 166, 241, 110, 115, 45, 234,
                                     234, 81, 59, 86, 139, 184, 78, 232, 1, 251, 90, 2, 231, 211, 45};
        std::memcpy(expected.begin(), expectedB, std::min(size_t(expected.size()), sizeof(expectedB)));
        BOOST_CHECK_MESSAGE(ParseHashStr("2dd3e7025afb01e84eb88b563b51eaea2d736ef1a6a0478a3a14c72c58c8a3fe", parsed)
                            && parsed == expected, "Parsing hash256 should yield the expected result");
    }
}

static std::string ConcatSighashStr(const std::string &s1, const std::string &s2) {
    if (s1.empty() || s2.empty() || s1.back() == '|' || s2.front() == '|') return s1 + s2;
    else return s1 + "|" + s2;
};

BOOST_AUTO_TEST_CASE(parse_sighash_str) {
    const std::map<std::string, uint32_t> tokenIntMap{{
        {"ALL", SIGHASH_ALL},
        {"NONE", SIGHASH_NONE},
        {"SINGLE", SIGHASH_SINGLE},
        {"FORKID", SIGHASH_FORKID},
        {"ANYONECANPAY", SIGHASH_ANYONECANPAY},
        {"UTXOS", SIGHASH_UTXOS},
        {"", 0},
    }};
    for (const auto &base : {"ALL", "NONE", "SINGLE"}) {
        auto it = tokenIntMap.find(base);
        assert(it != tokenIntMap.end());
        const uint32_t basePart = it->second;
        for (const auto &fid : {"", "FORKID"}) {
            it = tokenIntMap.find(fid);
            assert(it != tokenIntMap.end());
            const uint32_t fidPart = it->second;
            for (const auto &modifier : {"", "ANYONECANPAY", "UTXOS"}) {
                it = tokenIntMap.find(modifier);
                assert(it != tokenIntMap.end());
                const uint32_t modifierPart = it->second;
                const auto str = ConcatSighashStr(ConcatSighashStr(base, fid), modifier);
                const SigHashType expected(basePart | fidPart | modifierPart);
                const SigHashType parsed = ParseSighashString(UniValue{str});
                BOOST_CHECK_MESSAGE(parsed == expected,
                                    strprintf("Testing \"%s\" -> Parsed: 0x%02x, Expected: 0x%02x",
                                              str, parsed.getRawSigHashType(), expected.getRawSigHashType()));
                // Now, ensure that converting the parsed value back to the string works as expected
                BOOST_CHECK_EQUAL(SighashToStr(parsed.getRawSigHashType()), str);
            }
        }
    }
}

// Tests both DecodeTokenDataUV and TokenDataToUniv functions at the same time (plus the functions that they call)
BOOST_AUTO_TEST_CASE(test_DecodeTokenDataUV_TokenDataToUniv) {
    auto MakeJsonString = [](const std::string &categoryStr, const std::string &amountStr,
                             const std::string &commitmentStr) -> std::string {
        return strprintf(R"""(
            {
                "category": "%s",
                "amount": %s,
                "nft": {
                    "capability": "minting",
                    "commitment": "%s"
                }
            }
        )""", categoryStr, amountStr, commitmentStr);
    };

    const auto commitment = ParseHex("0102030405060708090a0b0c0d0e0f1122334456789a");
    const uint256 category = InsecureRand256();

    UniValue uv;
    {
        const std::string json = MakeJsonString(category.ToString(), "\"9223372036854775807\"", HexStr(commitment));

        // first test the above
        BOOST_REQUIRE(uv.read(json));
    }

    // test basic parse works as expected
    {
        const token::OutputData tok = DecodeTokenDataUV(uv);
        BOOST_CHECK(tok.GetId() == category);
        BOOST_CHECK(tok.HasNFT());
        BOOST_CHECK( ! tok.IsMutableNFT());
        BOOST_CHECK( ! tok.IsImmutableNFT());
        BOOST_CHECK(tok.IsMintingNFT());
        BOOST_CHECK(Span{tok.GetCommitment()} == Span{commitment});
        BOOST_CHECK(tok.GetAmount().getint64() == 9223372036854775807LL);

        // check round-trip to UniValue and back preserves the same data
        const UniValue::Object o = TokenDataToUniv(tok);
        auto *amount = o.locate("amount");
        BOOST_REQUIRE(amount != nullptr);
        BOOST_CHECK(amount->isStr()); // all amounts should encode as strings
        const token::OutputData tok2 = DecodeTokenDataUV(UniValue{o});
        BOOST_CHECK(tok == tok2);
    }


    // next, run through the various capabilities and test those
    for (const std::string capability : {"", "none", "mutable"}) {
        UniValue uvCopy{uv};
        UniValue *nft_uv = uvCopy.locate("nft");
        BOOST_REQUIRE(nft_uv != nullptr && nft_uv->isObject());
        UniValue::Object &nft = nft_uv->get_obj();
        UniValue *cap = nft.locate("capability");
        BOOST_REQUIRE(cap != nullptr);
        if (capability.empty()) {
            // delete capability key to test missing key works
            cap = nullptr;
            auto it = std::find_if(nft.begin(), nft.end(), [](const auto &p){ return p.first == "capability"; });
            BOOST_REQUIRE(it != nft.end());
            nft.erase(it, it + 1);
        } else {
            // otherwise just set the capability value to "none" or "mutable"
            cap->get_str() = capability;
        }

        const token::OutputData tok = DecodeTokenDataUV(uvCopy);
        BOOST_CHECK(tok.GetId() == category);
        BOOST_CHECK(tok.HasNFT());
        BOOST_CHECK(tok.IsMutableNFT() == (capability == "mutable"));
        BOOST_CHECK(tok.IsImmutableNFT() == (capability == "" || capability == "none"));
        BOOST_CHECK( ! tok.IsMintingNFT());
        BOOST_CHECK(Span{tok.GetCommitment()} == Span{commitment});
        BOOST_CHECK(tok.GetAmount().getint64() == 9223372036854775807LL);

        // check round-trip to UniValue and back preserves the same data
        const token::OutputData tok2 = DecodeTokenDataUV(TokenDataToUniv(tok));
        BOOST_CHECK(tok == tok2);
    }

    // next, test fungible-only case
    {
        UniValue uvCopy{uv};
        UniValue::Object &o = uvCopy.get_obj();
        auto it = std::find_if(o.begin(), o.end(), [](const auto &p){ return p.first == "nft"; }); // erase nft section
        BOOST_REQUIRE(it != o.end());
        o.erase(it, it + 1);

        const token::OutputData tok = DecodeTokenDataUV(uvCopy);
        BOOST_CHECK(tok.GetId() == category);
        BOOST_CHECK( ! tok.HasNFT());
        BOOST_CHECK( ! tok.IsMutableNFT());
        BOOST_CHECK( ! tok.IsImmutableNFT());
        BOOST_CHECK( ! tok.IsMintingNFT());
        BOOST_CHECK(tok.IsFungibleOnly());
        BOOST_CHECK(tok.GetCommitment().empty());
        BOOST_CHECK(tok.GetAmount().getint64() == 9223372036854775807LL);

        // check round-trip to UniValue and back preserves the same data
        const token::OutputData tok2 = DecodeTokenDataUV(TokenDataToUniv(tok));
        BOOST_CHECK(tok == tok2);
    }

    // next, test amount parsing where the "amount" is not a string but a numeric
    {
        const std::string json = MakeJsonString(category.ToString(), "9223372036854775807", HexStr(commitment));
        UniValue uvAlt;
        BOOST_REQUIRE(uvAlt.read(json));
        BOOST_CHECK(uvAlt != uv);

        const token::OutputData tok = DecodeTokenDataUV(uv);
        const token::OutputData tokAlt = DecodeTokenDataUV(uvAlt);
        // even though we encoded amount as bare 9223372036854775807 in the JSON, it should parse ok
        BOOST_CHECK(tok == tokAlt);

        // check round-trip to UniValue and back preserves the same data
        UniValue::Object o = TokenDataToUniv(tok);
        auto *amount = o.locate("amount");
        BOOST_REQUIRE(amount != nullptr);
        BOOST_CHECK(amount->isStr()); // all amounts should encode as strings always
        token::OutputData tok2 = DecodeTokenDataUV(UniValue{o});
        BOOST_CHECK(tok == tok2);

        // check amount that is small also is always a string
        tok2.SetAmount(token::SafeAmount::fromInt(1).value());
        o = TokenDataToUniv(tok2);
        amount = o.locate("amount");
        BOOST_REQUIRE(amount != nullptr);
        BOOST_CHECK(amount->isStr()); // all amounts should encode as strings always, even if small
    }

    // next, test amount parsing where the "amount" is negative (should fail)
    {
        const std::string json = MakeJsonString(category.ToString(), "\"-1\"", HexStr(commitment));

        UniValue uvAlt;
        BOOST_REQUIRE(uvAlt.read(json));
        token::OutputData tok;
        BOOST_CHECK_THROW(tok = DecodeTokenDataUV(uvAlt), std::runtime_error);
    }

    // next, test amount parsing where the "amount" is out of range (should fail)
    {
        const std::string json = MakeJsonString(category.ToString(), "\"9223372036854775808\"", HexStr(commitment));

        UniValue uvAlt;
        BOOST_REQUIRE(uvAlt.read(json));
        token::OutputData tok;
        BOOST_CHECK_THROW(tok = DecodeTokenDataUV(uvAlt), std::runtime_error);
    }

    // next, test amount parsing where the "amount" is 0 but the token has no NFT (should fail)
    {
        const std::string json = MakeJsonString(category.ToString(), "0", HexStr(commitment));

        UniValue uvAlt;
        BOOST_REQUIRE(uvAlt.read(json));
        {
            UniValue::Object &o = uvAlt.get_obj();
            // erase nft section to make it fungible-only
            auto it = std::find_if(o.begin(), o.end(), [](const auto &p){ return p.first == "nft"; });
            BOOST_REQUIRE(it != o.end());
            o.erase(it, it + 1);
        }

        token::OutputData tok;
        BOOST_CHECK_THROW(tok = DecodeTokenDataUV(uvAlt), std::runtime_error);
    }

    // next, test commitment parsing where the "commitment" is too long
    {
        auto commitmentCopy = commitment;
        commitmentCopy.insert(commitmentCopy.end(), commitment.begin(), commitment.end());
        BOOST_REQUIRE(commitmentCopy.size() > token::MAX_CONSENSUS_COMMITMENT_LENGTH);

        const std::string json = MakeJsonString(category.ToString(), "\"1\"", HexStr(commitmentCopy));

        UniValue uvAlt;
        BOOST_REQUIRE(uvAlt.read(json));
        token::OutputData tok;
        BOOST_CHECK_THROW(tok = DecodeTokenDataUV(uvAlt), std::runtime_error);
    }

    // next, test commitment parsing where the "commitment" is not hex
    {
        const std::string json = MakeJsonString(category.ToString(), "\"1\"", "aabbzz");

        UniValue uvAlt;
        BOOST_REQUIRE(uvAlt.read(json));
        token::OutputData tok;
        BOOST_CHECK_THROW(tok = DecodeTokenDataUV(uvAlt), std::runtime_error);
    }

    // next, test parsing where the "category" is not hex
    {
        const std::string json = MakeJsonString("aabbccddeeffgg", "\"1\"", HexStr(commitment));

        UniValue uvAlt;
        BOOST_REQUIRE(uvAlt.read(json));
        token::OutputData tok;
        BOOST_CHECK_THROW(tok = DecodeTokenDataUV(uvAlt), std::runtime_error);
    }

    // next, test parsing where the "category" is hex but is of the wrong length
    {
        const std::string json = MakeJsonString(category.ToString() + "00", "\"1\"", HexStr(commitment));

        UniValue uvAlt;
        BOOST_REQUIRE(uvAlt.read(json));
        token::OutputData tok;
        BOOST_CHECK_THROW(tok = DecodeTokenDataUV(uvAlt), std::runtime_error);
    }

    // next, test parsing where the "category" is missing
    {
        const std::string json = MakeJsonString(category.ToString(), "\"1\"", HexStr(commitment));

        UniValue uvAlt;
        BOOST_REQUIRE(uvAlt.read(json));

        {
            UniValue::Object &o = uvAlt.get_obj();
            // erase category
            auto it = std::find_if(o.begin(), o.end(), [](const auto &p){ return p.first == "category"; });
            BOOST_REQUIRE(it != o.end());
            o.erase(it, it + 1);
        }

        token::OutputData tok;
        BOOST_CHECK_THROW(tok = DecodeTokenDataUV(uvAlt), std::runtime_error);
    }
}

BOOST_AUTO_TEST_SUITE_END()
