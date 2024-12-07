// Copyright (c) 2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/setup_common.h>

#include <primitives/transaction.h>
#include <random.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstring>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(bip69_tests, TestingSetup)

static std::vector<uint8_t> GetRandBytes(const size_t n) {
    std::vector <uint8_t> ret;
    ret.resize(n);
    size_t pos = 0;
    while (n - pos > 0) {
        const auto h = InsecureRand256();
        const size_t n2Copy = std::min<size_t>(h.size(), n - pos);
        std::memcpy(ret.data() + pos, h.data(), n2Copy);
        pos += n2Copy;
    }
    return ret;
}

static bool IsTxSorted(const CMutableTransaction &tx) {
    // check outputs are sorted ascending according to: nValue, scriptPubKey
    for (size_t i = 1; i < tx.vout.size(); ++i) {
        auto &a = tx.vout[i-1], &b = tx.vout[i];
        if (a.nValue > b.nValue) {
            return false;
        } else if (a.nValue == b.nValue && a.scriptPubKey == b.scriptPubKey && b.tokenDataPtr < a.tokenDataPtr) {
            return false;
        } else if (a.nValue == b.nValue && a.scriptPubKey != b.scriptPubKey) {
            // Note: we cannot use CScript::operator< (which is really prevector::operator<) since
            // it does not order things lexicographically.  So, we must do this:
            const int cmp = std::memcmp(a.scriptPubKey.data(), b.scriptPubKey.data(),
                                        std::min(a.scriptPubKey.size(), b.scriptPubKey.size()));
            if (cmp > 0 || (cmp == 0 && a.scriptPubKey.size() > b.scriptPubKey.size())) {
                return false;
            }
        }
    }
    // check inputs are sorted ascending according to COutpoint::operator<
    for (size_t i = 1; i < tx.vin.size(); ++i) {
        auto &a = tx.vin[i-1].prevout, &b = tx.vin[i].prevout;
        if (!(a < b || a == b)) {
            return false;
        }
    }

    return true;
}

BOOST_AUTO_TEST_CASE(random_tests) {
    // Create a txn with completely random inputs, random outputs, sort it, test sorting
    {
        CMutableTransaction tx;

        tx.vin.resize(100);
        tx.vout.resize(100);

        // Completely random input hashes with random input index [0, 100]
        for (auto &in : tx.vin) {
            in.prevout = COutPoint(TxId{InsecureRand256()}, GetRand(100));
        }
        // Completely random output value in range [0, 100] BCH, random scriptPubKey data of random length [0, 32]
        // and a random token.
        for (auto &out : tx.vout) {
            out.nValue = int64_t(GetRand(100)) * COIN;
            const auto vec = GetRandBytes(GetRand(32));
            out.scriptPubKey.assign(vec.begin(), vec.end());
            const auto vec2 = GetRandBytes(GetRand(40));
            const bool hasNFT = !vec2.empty() || GetRand(2) == 1;
            const bool isMutable = hasNFT && GetRand(2) == 1;
            out.tokenDataPtr.emplace(token::Id{InsecureRand256()}, token::SafeAmount::fromIntUnchecked(GetRand(123456)),
                                     token::NFTCommitment{vec2.begin(), vec2.end()},
                                     hasNFT, // has nft
                                     isMutable,  // mutable
                                     !isMutable && GetRand(2) == 1); // minting
        }

        BOOST_CHECK_MESSAGE( ! IsTxSorted(tx), "Tx should not be sorted after random generation");
        tx.SortBip69();
        BOOST_CHECK_MESSAGE(IsTxSorted(tx), "Tx should now be sorted after calling SortBip69");
    }

    // Create a txn with random inputs that have all index 0, random output amounts but static spk
    {
        CMutableTransaction tx;

        tx.vin.resize(100);
        tx.vout.resize(100);

        // Completely random input hashes all with input index 0
        for (auto &in : tx.vin) {
            in.prevout = COutPoint(TxId{InsecureRand256()}, 0);
        }
        // Completely random output value in range [0, 100] BCH, static trivial spk
        for (auto &out : tx.vout) {
            out.nValue = int64_t(GetRand(100)) * COIN;
            out.scriptPubKey = CScript() << OP_1 << OP_2 << OP_3 << OP_4 << OP_DROP << OP_DROP << OP_DROP;
        }

        BOOST_CHECK_MESSAGE( ! IsTxSorted(tx), "Tx should not be sorted after random generation");
        tx.SortBip69();
        BOOST_CHECK_MESSAGE(IsTxSorted(tx), "Tx should now be sorted after calling SortBip69");
    }

    // Create a txn with:
    // - inputs that all have same hash, but random inputN
    // - outputs that pay to random scriptPubKeys of length [0, 32], but all have same value
    {
        CMutableTransaction tx;

        tx.vin.resize(100);
        tx.vout.resize(100);

        // Static input hash with random input index [0, 10,000)
        const TxId theId{InsecureRand256()};
        for (auto &in : tx.vin) {
            in.prevout = COutPoint(theId, GetRand(10'000));
        }
        // Completely random output value, random scriptPubKey data of random length [0, 32]
        const Amount amt = int64_t(GetRand(1234567890)) * SATOSHI;
        for (auto &out : tx.vout) {
            out.nValue = amt;
            const auto vec = GetRandBytes(GetRand(32));
            out.scriptPubKey.assign(vec.begin(), vec.end());
        }

        BOOST_CHECK_MESSAGE( ! IsTxSorted(tx), "Tx should not be sorted after random generation");
        tx.SortBip69();
        BOOST_CHECK_MESSAGE(IsTxSorted(tx), "Tx should now be sorted after calling SortBip69");
    }

    // Create a txn with:
    // - inputs that all have same hash, but random inputN
    // - outputs that pay to a scriptPubKeys of length [0, 32] that is a subtring of a static scriptPubKey of length 32,
    //   and all have same value.
    {
        CMutableTransaction tx;

        tx.vin.resize(100);
        tx.vout.resize(100);

        // Static input hash with random input index [0, 10,000]
        const TxId theId{InsecureRand256()};
        for (auto &in : tx.vin) {
            in.prevout = COutPoint(theId, GetRand(10'000));
        }
        // Completely random output value, scriptPubKey subscript data of random length [0, 32]
        const Amount amt = int64_t(GetRand(1234567890)) * SATOSHI;
        const auto vec = GetRandBytes(32);
        for (auto &out : tx.vout) {
            out.nValue = amt;
            // each spk is a randomly-sized subscript of `vec` above
            out.scriptPubKey.assign(vec.begin(), vec.begin() + GetRand(uint64_t(vec.size())));
        }

        BOOST_CHECK_MESSAGE( ! IsTxSorted(tx), "Tx should not be sorted after random generation");
        tx.SortBip69();
        BOOST_CHECK_MESSAGE(IsTxSorted(tx), "Tx should now be sorted after calling SortBip69");
    }

    // Create a txn with:
    // - inputs that all have same hash, but random inputN
    // - outputs that pay to a static scriptPubKey, but with random token NFT that is a prefix of a static byte str
    {
        CMutableTransaction tx;

        tx.vin.resize(100);
        tx.vout.resize(100);

        // Static input hash with random input index [0, 10,000]
        const TxId theId{InsecureRand256()};
        for (auto &in : tx.vin) {
            in.prevout = COutPoint(theId, GetRand(10'000));
        }
        // Completely random output value, scriptPubKey subscript data of random length [0, 32]
        const Amount amt = int64_t(GetRand(1234567890)) * SATOSHI;
        const auto vec = GetRandBytes(32);
        for (auto &out : tx.vout) {
            out.nValue = amt;
            out.scriptPubKey.assign(vec.begin(), vec.end());
            out.tokenDataPtr.emplace(token::Id(uint256{vec}), token::SafeAmount::fromIntUnchecked(20),
                                     token::NFTCommitment{vec.begin(), vec.begin() + GetRand(vec.size())},
                                     true, true, true);
        }

        BOOST_CHECK_MESSAGE( ! IsTxSorted(tx), "Tx should not be sorted after random generation");
        tx.SortBip69();
        BOOST_CHECK_MESSAGE(IsTxSorted(tx), "Tx should now be sorted after calling SortBip69");
    }
}

BOOST_AUTO_TEST_SUITE_END()
