// Copyright (c) 2018-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/sighashtype.h>

#include <streams.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(sighashtype_tests, BasicTestingSetup)

static void CheckSigHashType(SigHashType t,
                             BaseSigHashType baseType,
                             bool isDefined,
                             bool hasFork,
                             bool hasAnyoneCanPay,
                             bool hasUtxos) {
    BOOST_CHECK(t.getBaseType() == baseType);
    BOOST_CHECK_EQUAL(t.isDefined(), isDefined);
    BOOST_CHECK_EQUAL(t.hasFork(), hasFork);
    BOOST_CHECK_EQUAL(t.hasAnyoneCanPay(), hasAnyoneCanPay);
    BOOST_CHECK_EQUAL(t.hasUtxos(), hasUtxos);
}

BOOST_AUTO_TEST_CASE(sighash_construction_test) {
    // Check default values.
    CheckSigHashType(SigHashType(),
                     BaseSigHashType::ALL,
                     true,   // isDefined
                     false,  // hasFork
                     false,  // hasAnyoneCanPay
                     false); // hasUtxos

    // Check all possible permutations.
    const BaseSigHashType baseTypes[] = {BaseSigHashType::UNSUPPORTED,
                                         BaseSigHashType::ALL,
                                         BaseSigHashType::NONE,
                                         BaseSigHashType::SINGLE};
    for (BaseSigHashType baseType : baseTypes) {
        for (const bool hasFork : {false, true}) {
            for (const bool hasAnyoneCanPay : {false, true}) {
                for (const bool hasUtxos : {false, true}) {
                    const SigHashType t = SigHashType().withBaseType(baseType)
                                                       .withFork(hasFork)
                                                       .withAnyoneCanPay(hasAnyoneCanPay)
                                                       .withUtxos(hasUtxos);

                    const bool isDefined = baseType != BaseSigHashType::UNSUPPORTED;
                    CheckSigHashType(t,
                                     baseType,
                                     isDefined,
                                     hasFork,
                                     hasAnyoneCanPay,
                                     hasUtxos);

                    // Also check all possible alterations.
                    CheckSigHashType(t.withFork(hasFork),
                                     baseType,
                                     isDefined,
                                     hasFork,
                                     hasAnyoneCanPay,
                                     hasUtxos);
                    CheckSigHashType(t.withFork(!hasFork),
                                     baseType,
                                     isDefined,
                                     !hasFork,
                                     hasAnyoneCanPay,
                                     hasUtxos);
                    CheckSigHashType(t.withAnyoneCanPay(hasAnyoneCanPay),
                                     baseType,
                                     isDefined,
                                     hasFork,
                                     hasAnyoneCanPay,
                                     hasUtxos);
                    CheckSigHashType(t.withAnyoneCanPay(!hasAnyoneCanPay),
                                     baseType,
                                     isDefined,
                                     hasFork,
                                     !hasAnyoneCanPay,
                                     hasUtxos);
                    CheckSigHashType(t.withUtxos(hasUtxos),
                                     baseType,
                                     isDefined,
                                     hasFork,
                                     hasAnyoneCanPay,
                                     hasUtxos);
                    CheckSigHashType(t.withUtxos(!hasUtxos),
                                     baseType,
                                     isDefined,
                                     hasFork,
                                     hasAnyoneCanPay,
                                     !hasUtxos);

                    for (BaseSigHashType newBaseType : baseTypes) {
                        const bool isNewDefined = newBaseType != BaseSigHashType::UNSUPPORTED;
                        CheckSigHashType(t.withBaseType(newBaseType),
                                         newBaseType,
                                         isNewDefined,
                                         hasFork,
                                         hasAnyoneCanPay,
                                         hasUtxos);
                    }
                }
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(sighash_serialization_test) {
    // Test all possible sig hash values embedded in signatures.
    for (uint32_t sigHashType = 0x00; sigHashType <= 0xff; sigHashType++) {
        uint32_t rawType = sigHashType;

        uint32_t baseType = rawType & 0x1f;
        bool hasFork = (rawType & SIGHASH_FORKID) != 0;
        bool hasAnyoneCanPay = (rawType & SIGHASH_ANYONECANPAY) != 0;
        bool hasUtxos = (rawType & SIGHASH_UTXOS) != 0;

        uint32_t noflag = sigHashType & ~(SIGHASH_FORKID | SIGHASH_ANYONECANPAY | SIGHASH_UTXOS);
        const bool isDefined = (noflag != 0) && (noflag <= SIGHASH_SINGLE);

        const SigHashType tbase(rawType);

        // Check deserialization.
        CheckSigHashType(tbase,
                         BaseSigHashType(baseType),
                         isDefined,
                         hasFork,
                         hasAnyoneCanPay,
                         hasUtxos);

        // Check raw value.
        BOOST_CHECK_EQUAL(tbase.getRawSigHashType(), rawType);

        // Check serialization/deserialization.
        uint32_t unserializedOutput;
        (CDataStream(SER_DISK, 0) << tbase) >> unserializedOutput;
        BOOST_CHECK_EQUAL(unserializedOutput, rawType);
    }
}

BOOST_AUTO_TEST_SUITE_END()
