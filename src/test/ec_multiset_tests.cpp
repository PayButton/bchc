// Copyright (c) 22 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <ec_multiset.h>

#include <streams.h>
#include <test/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <map>
#include <random>
#include <unordered_map>
#include <utility>

// Test vectors taken from:
// https://github.com/SoftwareVerde/java-cryptography/blob/master/src/test/java/com/softwareverde/cryptography/secp256k1/EcMultisetTests.java
BOOST_FIXTURE_TEST_SUITE(ec_multiset_tests, BasicTestingSetup)
    static const auto D1_BYTES = ParseHex("982051FD1E4BA744BBBE680E1FEE14677BA1A3C3540BF7B1CDB606E857233E0E00000000010000000100F2052A0100000043410496B538E853519C726A2C91E61EC11600AE1390813A627C66FB8BE7947BE63C52DA7589379515D4E0A604F8141781E62294721166BF621E73A82CBF2342C858EEAC");
    static const auto D2_BYTES = ParseHex("D5FDCC541E25DE1C7A5ADDEDF24858B8BB665C9F36EF744EE42C316022C90F9B00000000020000000100F2052A010000004341047211A824F55B505228E4C3D5194C1FCFAA15A456ABDF37F9B9D97A4040AFC073DEE6C89064984F03385237D92167C13E236446B417AB79A0FCAE412AE3316B77AC");
    static const auto D3_BYTES = ParseHex("44F672226090D85DB9A9F2FBFE5F0F9609B387AF7BE5B7FBB7A1767C831C9E9900000000030000000100F2052A0100000043410494B9D3E76C5B1629ECF97FFF95D7A4BBDAC87CC26099ADA28066C6FF1EB9191223CD897194A08D0C2726C5747F1DB49E8CF90E75DC3E3550AE9B30086F3CD5AAAC");

BOOST_AUTO_TEST_CASE(should_be_an_empty_hash_if_empty) {
    const uint256 expectedHash{}; // all zeroes
    const ECMultiSet::PubKeyBytes expectedPubKeyBytes = {{0x0u}}; // all zeroes
    const ECMultiSet emptySet;

    BOOST_CHECK(emptySet.GetPubKeyBytes() == expectedPubKeyBytes);
    BOOST_CHECK(emptySet.GetHash() == expectedHash);
}

// Since the test vector hex strings are in little endian, but uint256S expects big-endian hex, we must reverse what
// uint256S gives us
static uint256 uint256SRev(const char *str) {
    uint256 ret = uint256S(str);
    std::reverse(ret.begin(), ret.end());
    return ret;
}

BOOST_AUTO_TEST_CASE(should_calculate_multiset_hash_1) {
    // Setup
    const uint256 expectedValue = uint256SRev("F883195933A687170C34FA1ADEC66FE2861889279FB12C03A3FB0CA68AD87893");
    ECMultiSet ecm;

    // Action
    ecm.Add(D1_BYTES);

    // Assert
    const auto value = ecm.GetHash();
    BOOST_CHECK(expectedValue == value);
}

BOOST_AUTO_TEST_CASE(should_calculate_multiset_hash_2) {
    // Setup
    const uint256 expectedValue = uint256SRev("EF85D123A15DA95D8AFF92623AD1E1C9FCDA3BAA801BD40BC567A83A6FDCF3E2");
    ECMultiSet ecm;

    // Action
    ecm.Add(D2_BYTES);

    // Assert
    const auto value = ecm.GetHash();
    BOOST_CHECK(expectedValue == value);
}

BOOST_AUTO_TEST_CASE(should_calculate_multiset_hash_3) {
    // Setup
    const uint256 expectedValue = uint256SRev("CFADF40FC017FAFF5E04CCC0A2FAE0FD616E4226DD7C03B1334A7A610468EDFF");
    ECMultiSet ecm;

    // Action
    ecm.Add(D3_BYTES);

    // Assert
    const auto value = ecm.GetHash();
    BOOST_CHECK(expectedValue == value);
}

BOOST_AUTO_TEST_CASE(should_calculate_merged_multiset_hash_of_d1_and_d2) {
    // Setup
    const uint256 expectedValue = uint256SRev("FABAFD38D07370982A34547DAF5B57B8A4398696D6FD2294788ABDA07B1FAAAF");
    ECMultiSet ecm1, ecm2, alt_ecm1, alt_ecm2;

    // Action
    ecm1.Add(D1_BYTES);
    alt_ecm1 += D1_BYTES; // Also test += syntax (synonym for .Add() here)
    ecm2.Add(D2_BYTES);
    alt_ecm2 += D2_BYTES;

    ecm1.Combine(ecm2);
    alt_ecm1 += alt_ecm2; // // Also test += syntax (synonym for .Combine() here)

    // Assert
    const auto value = ecm1.GetHash();
    BOOST_CHECK(expectedValue == value);
    BOOST_CHECK(alt_ecm1.GetHash() == expectedValue);
    BOOST_CHECK(ecm1 == alt_ecm1); // test operator== on multisets
    BOOST_CHECK(ecm1 != ecm2); // test operator!= on multisets
}

BOOST_AUTO_TEST_CASE(should_calculate_multiset_hash_of_d1_and_d2) {
    // Setup
    const uint256 expectedValue = uint256SRev("FABAFD38D07370982A34547DAF5B57B8A4398696D6FD2294788ABDA07B1FAAAF");
    ECMultiSet ecm;

    // Test chained syntax
    ecm.Add(D1_BYTES).Add(D2_BYTES);

    // Assert
    const auto value = ecm.GetHash();
    BOOST_CHECK(expectedValue == value);
}

BOOST_AUTO_TEST_CASE(should_calculate_merged_multiset_hash_of_d1_d2_and_d3) {
    // Setup
    const uint256 expectedValue = uint256SRev("1CBCCDA23D7CE8C5A8B008008E1738E6BF9CFFB1D5B86A92A4E62B5394A636E2");
    ECMultiSet ecm, d2ecm,
               d3ecm{D3_BYTES}; // test constructor syntax

    ecm.Add(D1_BYTES);
    d2ecm = d2ecm + D2_BYTES; // test + operator (as opposed to +=)
    // d3ecm.Add(D3_BYTES);  // already done above in c'tor

    ecm.Combine(d2ecm).Combine(d3ecm);  // combine all 3 into -> ecm, using chained syntax

    // Assert
    const auto value = ecm.GetHash();
    BOOST_CHECK(expectedValue == value);
}

BOOST_AUTO_TEST_CASE(should_calculate_multiset_hash_of_d1_d2_and_d3) {
    // Setup
    const uint256 expectedValue = uint256SRev("1CBCCDA23D7CE8C5A8B008008E1738E6BF9CFFB1D5B86A92A4E62B5394A636E2");
    ECMultiSet ecm;

    // Action
    ecm.Add(D1_BYTES);
    ecm.Add(D2_BYTES);
    ecm.Add(D3_BYTES);
    ECMultiSet ecm2{ecm}; // check copy c'tor
    const auto value2 = ecm2.GetHash(); // save hash
    ECMultiSet *pecm2 = &ecm2; // save pointer to ecm2 to avoid use-after-move warnings from compiler below
    ECMultiSet ecm3{std::move(ecm2)}; // check move c'tor
    const auto value3 = ecm3.GetHash(); // save hash

    // Assert
    const auto value = ecm.GetHash();
    BOOST_CHECK(expectedValue == value);
    BOOST_CHECK(expectedValue == value2);
    BOOST_CHECK(expectedValue == value3);
    BOOST_CHECK(pecm2->IsEmpty()); // after a move-from we expect the set to be empty
}

BOOST_AUTO_TEST_CASE(should_calculate_multiset_hash_of_d1_d2_after_adding_and_removing_d3) {
    // Setup
    const uint256 expectedValue = uint256SRev("FABAFD38D07370982A34547DAF5B57B8A4398696D6FD2294788ABDA07B1FAAAF");
    ECMultiSet ecm;

    // Action
    ecm.Add(D1_BYTES).Add(D2_BYTES).Add(D3_BYTES);
    ECMultiSet ecm2{ecm};
    ecm.Remove(D3_BYTES);
    ecm2 -= D3_BYTES; // test -=, synonym for .Remove()

    // Assert
    const auto value = ecm.GetHash();
    const auto value2 = ecm2.GetHash();
    BOOST_CHECK(expectedValue == value);
    BOOST_CHECK(expectedValue == value2);
}

BOOST_AUTO_TEST_CASE(should_calculate_merged_multiset_hash_of_d1p_d2p_and_d3p) {
    // Setup
    const uint256 expectedValue = uint256SRev("1CBCCDA23D7CE8C5A8B008008E1738E6BF9CFFB1D5B86A92A4E62B5394A636E2");
    const ECMultiSet::PubKeyBytes pkb0 = {{0x0u}};

    const CPubKey d1pk = []{
        ECMultiSet ecm;
        ecm.Add(D1_BYTES);
        return ecm.GetPubKey();
    }();
    BOOST_REQUIRE(d1pk.IsFullyValid() && d1pk.IsCompressed());

    const CPubKey d2pk = []{
        ECMultiSet ecm;
        ecm.Add(D2_BYTES);
        return ecm.GetPubKey();
    }();
    BOOST_REQUIRE(d2pk.IsFullyValid() && d2pk.IsCompressed());

    const CPubKey d3pk = []{
        ECMultiSet ecm;
        ecm.Add(D3_BYTES);
        return ecm.GetPubKey();
    }();
    BOOST_REQUIRE(d3pk.IsFullyValid() && d3pk.IsCompressed());

    const ECMultiSet::PubKeyBytes d1pkb = []{
        ECMultiSet ecm;
        ecm.Add(D1_BYTES);
        return ecm.GetPubKeyBytes();
    }();
    BOOST_REQUIRE(d1pkb != pkb0);

    const ECMultiSet::PubKeyBytes d2pkb = []{
        ECMultiSet ecm;
        ecm.Add(D2_BYTES);
        return ecm.GetPubKeyBytes();
    }();
    BOOST_REQUIRE(d2pkb != pkb0);

    const ECMultiSet::PubKeyBytes d3pkb = []{
        ECMultiSet ecm;
        ecm.Add(D3_BYTES);
        return ecm.GetPubKeyBytes();
    }();
    BOOST_REQUIRE(d3pkb != pkb0);

    // Action
    const ECMultiSet ecm = ECMultiSet(d1pk).Combine(ECMultiSet(d2pk)).Combine(ECMultiSet(d3pk));
    const ECMultiSet ecm2 = ECMultiSet(d1pkb).Combine(ECMultiSet(d2pkb)).Combine(ECMultiSet(d3pkb));

    // Assert
    const auto value = ecm.GetHash();
    const auto value2 = ecm2.GetHash();
    BOOST_CHECK(expectedValue == value);
    BOOST_CHECK(expectedValue == value2);
    BOOST_CHECK(ecm == ecm2);
    BOOST_CHECK(ecm.GetPubKey() == ecm2.GetPubKey());
    BOOST_CHECK(ecm.GetPubKeyBytes() == ecm2.GetPubKeyBytes());
    BOOST_CHECK(ECMultiSet(ecm.GetPubKey()) == ecm);
    BOOST_CHECK(ECMultiSet(ecm.GetPubKeyBytes()) == ecm);
}

BOOST_AUTO_TEST_CASE(should_noop_when_adding_two_empty_sets) {
    // Setup
    const uint256 expectedValue{}; // all zeroes
    const ECMultiSet::PubKeyBytes pkb0 = {{0x0u}};
    const ECMultiSet emptyEcm;
    ECMultiSet ecm;

    // Action
    ecm.Combine(emptyEcm);

    // Assert
    const auto value = ecm.GetHash();
    const auto pkbValue = ecm.GetPubKeyBytes();
    BOOST_CHECK(expectedValue == value);
    BOOST_CHECK(pkbValue == pkb0);
    BOOST_CHECK(ecm.IsEmpty());
    BOOST_CHECK(emptyEcm.IsEmpty());
    BOOST_CHECK(ecm == emptyEcm);
    BOOST_CHECK(ECMultiSet(pkbValue) == emptyEcm);
}

BOOST_AUTO_TEST_CASE(should_noop_when_adding_empty_pk) {
    // Setup
    const uint256 expectedValue{}; // all zeroes
    const ECMultiSet::PubKeyBytes pkb0 = {{0x0u}};
    const ECMultiSet emptyEcm;
    ECMultiSet ecm;

    ecm.Combine(ECMultiSet(emptyEcm.GetPubKey()));
    // Assert
    const auto value = ecm.GetHash();
    BOOST_CHECK(expectedValue == value);
    BOOST_CHECK(ecm.GetPubKeyBytes() == pkb0);
    BOOST_CHECK(ecm.IsEmpty());
    BOOST_CHECK(ecm == emptyEcm);
}

BOOST_AUTO_TEST_CASE(unserialize_zeroes_should_work) {
    // Setup
    const ECMultiSet::PubKeyBytes pkb0 = {{0x0u}};
    ECMultiSet ecm;

    ecm += D1_BYTES;

    BOOST_REQUIRE(!ecm.IsEmpty());
    BOOST_REQUIRE(ecm.GetPubKeyBytes() != pkb0);

    VectorReader(SER_NETWORK, PROTOCOL_VERSION, {pkb0.begin(), pkb0.end()}, 0) >> ecm;

    // Assert
    BOOST_CHECK(ecm.GetPubKeyBytes() == pkb0);
    BOOST_CHECK(ecm.IsEmpty());
}

BOOST_AUTO_TEST_CASE(unserialize_zero_prefix_but_nonzero_everything_else_should_not_work) {
    // Setup
    const ECMultiSet::PubKeyBytes pkb01 = {{0x0u, 0x01u, 0x2u, 0x3u, 0x4, 0x5u, 0x6u}};
    ECMultiSet ecm;

    ecm += D1_BYTES;

    BOOST_REQUIRE(!ecm.IsEmpty());
    BOOST_REQUIRE(ecm.GetPubKeyBytes() != pkb01);

    // Assert
    BOOST_REQUIRE_THROW(VectorReader(SER_NETWORK, PROTOCOL_VERSION, {pkb01.begin(), pkb01.end()}, 0) >> ecm,
                        std::ios_base::failure);
    BOOST_CHECK(ecm.GetPubKeyBytes() != pkb01);
    BOOST_CHECK(ecm.IsEmpty()); // after failing a deser, the object should be cleared
}

// Get random bytes randomly sized up to sizeLimit bytes in size
static std::vector<uint8_t> GetRandomData(const size_t sizeLimit = 8192u) {
    size_t randLimit;
    do {
        randLimit = InsecureRandRange(sizeLimit);
    } while (!randLimit);
    std::vector<uint8_t> ret;
    ret.reserve(randLimit);
    while (ret.size() < randLimit) {
        const auto h = InsecureRand256();
        const std::vector<uint8_t> blob(h.begin(), h.end());
        size_t n = blob.size();
        if (const auto nb = n + ret.size(); nb > randLimit) {
            n -= nb - randLimit;
        }
        ret.insert(ret.end(), blob.begin(), blob.begin() + n);
    }
    return ret;
}

BOOST_AUTO_TEST_CASE(randomized_serialize_unserialize_round_trip_tests) {
    constexpr unsigned nIters = 5u, nIters2Limit = 2000u;
    size_t totalIters = 0;

    for (unsigned i = 0; i < nIters; ++i) {
        ECMultiSet ecm, ecm2{InsecureRand256() /* ensure ecm2 != ecm to start */};

        BOOST_CHECK(ecm.IsEmpty());
        const unsigned nIters2 = InsecureRandRange(nIters2Limit);
        std::vector<std::vector<uint8_t>> randomData;
        randomData.reserve(nIters2);
        for (unsigned j = 0; j < nIters2; ++j, ++totalIters) {
            // loop a random number of times, inserting random data into the set
            std::vector<uint8_t> ser;
            BOOST_REQUIRE(ecm != ecm2);
            BOOST_REQUIRE(ecm.GetHash() != ecm2.GetHash());
            BOOST_REQUIRE(ecm.GetPubKey() != ecm2.GetPubKey());
            BOOST_REQUIRE(ecm.GetPubKeyBytes() != ecm2.GetPubKeyBytes());
            // test serialization -> unserialization cycle leads to the same equivalent set produced
            CVectorWriter(SER_NETWORK, PROTOCOL_VERSION, ser, 0) << ecm;
            VectorReader(SER_NETWORK, PROTOCOL_VERSION, ser, 0) >> ecm2;
            BOOST_REQUIRE(ecm == ecm2);
            BOOST_REQUIRE(ecm.GetHash() == ecm2.GetHash());
            BOOST_REQUIRE(ecm.GetPubKey() == ecm2.GetPubKey());
            BOOST_REQUIRE(ecm.GetPubKeyBytes() == ecm2.GetPubKeyBytes());
            // save random data to vector and add to set as well
            ecm += randomData.emplace_back(GetRandomData());
        }

        // next randomly shuffle the data and remove 1 item at a time from the set in the new random order
        {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(randomData.begin(), randomData.end(), g);
        }
        ecm2 += InsecureRand256(); // ensure ecm2 != ecm
        for (const auto &bytes : randomData) {
            std::vector<uint8_t> ser;
            BOOST_REQUIRE(ecm != ecm2);
            BOOST_REQUIRE(ecm.GetHash() != ecm2.GetHash());
            BOOST_REQUIRE(ecm.GetPubKey() != ecm2.GetPubKey());
            BOOST_REQUIRE(ecm.GetPubKeyBytes() != ecm2.GetPubKeyBytes());
            // test serialization -> unserialization cycle leads to the same equivalent set produced
            CVectorWriter(SER_NETWORK, PROTOCOL_VERSION, ser, 0) << ecm;
            VectorReader(SER_NETWORK, PROTOCOL_VERSION, ser, 0) >> ecm2;
            BOOST_REQUIRE(ecm == ecm2);
            BOOST_REQUIRE(ecm.GetHash() == ecm2.GetHash());
            BOOST_REQUIRE(ecm.GetPubKey() == ecm2.GetPubKey());
            BOOST_REQUIRE(ecm.GetPubKeyBytes() == ecm2.GetPubKeyBytes());
            // remove random data from set
            ecm -= bytes;
        }

        BOOST_REQUIRE(ecm.IsEmpty());
        BOOST_REQUIRE(ecm.GetHash() == uint256{});
        const ECMultiSet::PubKeyBytes pkb0 = {{0x0u}};
        BOOST_REQUIRE(ecm.GetPubKeyBytes() == pkb0);
    }

    BOOST_REQUIRE(totalIters > 0u);
}

BOOST_AUTO_TEST_CASE(std_map_and_unordered_map_key_tests) {
    constexpr size_t nIters = 2000u, nIters2 = 10u;
    using DataBlob = std::vector<uint8_t>;
    std::vector<std::pair<ECMultiSet, DataBlob>> ecms;
    std::map<ECMultiSet, DataBlob> map_ecms;
    std::unordered_map<ECMultiSet, DataBlob, SaltedECMultiSetHasher> umap_ecms;

    // The ECMultiSet should support being a map key
    for (size_t i = 0; i < nIters; ++i) {
        auto & [ecm, data] = ecms.emplace_back(std::piecewise_construct, std::forward_as_tuple(), std::forward_as_tuple());
        for (size_t j = 0; j < nIters2; ++j) {
            ecm.Add(GetRandomData(512));
        }
        data = GetRandomData();

        // save to map
        const auto & [it, inserted] = map_ecms.try_emplace(ecm, data);
        BOOST_REQUIRE(inserted);

        // save to unordered_map
        const auto & [it2, inserted2] = umap_ecms.try_emplace(ecm, data);
        BOOST_REQUIRE(inserted2);
    }

    BOOST_CHECK_EQUAL(ecms.size(), nIters);
    BOOST_CHECK_EQUAL(ecms.size(), map_ecms.size());
    BOOST_CHECK_EQUAL(ecms.size(), umap_ecms.size());

    // Next, look in the maps for the ECMultiSets from the vector. They should all be there as valid keys, and the data
    // for the keys should be what we put into the map in the first place.
    for (const auto & [ecm, data] : ecms) {
        const auto it = map_ecms.find(ecm);
        BOOST_REQUIRE(it != map_ecms.end());
        BOOST_CHECK(it->first == ecm);
        BOOST_CHECK(it->first.GetHash() == ecm.GetHash());
        BOOST_CHECK(it->first.GetPubKeyBytes() == ecm.GetPubKeyBytes());
        BOOST_CHECK(it->second == data);

        const auto it2 = umap_ecms.find(ecm);
        BOOST_REQUIRE(it2 != umap_ecms.end());
        BOOST_CHECK(it2->first == ecm);
        BOOST_CHECK(it2->first.GetHash() == ecm.GetHash());
        BOOST_CHECK(it2->first.GetPubKeyBytes() == ecm.GetPubKeyBytes());
        BOOST_CHECK(it2->second == data);
    }
}

BOOST_AUTO_TEST_SUITE_END()
