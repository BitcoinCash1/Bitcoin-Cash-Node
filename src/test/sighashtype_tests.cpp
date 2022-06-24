// Copyright (c) 2018-2019 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/sighashtype.h>

#include <streams.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <set>

BOOST_FIXTURE_TEST_SUITE(sighashtype_tests, BasicTestingSetup)

static void CheckSigHashType(SigHashType t,
                             BaseSigHashType baseType,
                             bool isDefined,
                             bool hasForkId,
                             bool hasAnyoneCanPay) {
    BOOST_CHECK(t.getBaseType() == baseType);
    BOOST_CHECK_EQUAL(t.isDefined(), isDefined);
    BOOST_CHECK_EQUAL(t.hasForkId(), hasForkId);
    BOOST_CHECK_EQUAL(t.hasAnyoneCanPay(), hasAnyoneCanPay);
}

BOOST_AUTO_TEST_CASE(sighash_construction_test) {
    // Check default values.
    CheckSigHashType(SigHashType(),
                     BaseSigHashType::ALL,
                     true, // isDefined
                     false, // hasForkId
                     false); // hasAnyoneCanPay

    // Check all possible permutations.
    std::set<BaseSigHashType> baseTypes{
        BaseSigHashType::UNSUPPORTED, BaseSigHashType::ALL,
        BaseSigHashType::NONE, BaseSigHashType::SINGLE};
    std::set<bool> forkIdFlagValues{false, true};
    std::set<bool> anyoneCanPayFlagValues{false, true};

    for (BaseSigHashType baseType : baseTypes) {
        for (bool hasForkId : forkIdFlagValues) {
            for (bool hasAnyoneCanPay : anyoneCanPayFlagValues) {
                const SigHashType t =
                    SigHashType()
                        .withBaseType(baseType)
                        .withForkId(hasForkId)
                        .withAnyoneCanPay(hasAnyoneCanPay);

                bool isDefined = baseType != BaseSigHashType::UNSUPPORTED;
                CheckSigHashType(t,
                                 baseType,
                                 isDefined,
                                 hasForkId,
                                 hasAnyoneCanPay);

                // Also check all possible alterations.
                CheckSigHashType(t.withForkId(hasForkId),
                                 baseType,
                                 isDefined,
                                 hasForkId,
                                 hasAnyoneCanPay);
                CheckSigHashType(t.withForkId(!hasForkId),
                                 baseType,
                                 isDefined,
                                 !hasForkId,
                                 hasAnyoneCanPay);
                CheckSigHashType(t.withAnyoneCanPay(hasAnyoneCanPay),
                                 baseType,
                                 isDefined,
                                 hasForkId,
                                 hasAnyoneCanPay);
                CheckSigHashType(t.withAnyoneCanPay(!hasAnyoneCanPay),
                                 baseType,
                                 isDefined,
                                 hasForkId,
                                 !hasAnyoneCanPay);

                for (BaseSigHashType newBaseType : baseTypes) {
                    bool isNewDefined =
                        newBaseType != BaseSigHashType::UNSUPPORTED;
                    CheckSigHashType(t.withBaseType(newBaseType),
                                     newBaseType,
                                     isNewDefined,
                                     hasForkId,
                                     hasAnyoneCanPay);
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
        bool hasForkId = (rawType & SIGHASH_FORKID) != 0;
        bool hasAnyoneCanPay = (rawType & SIGHASH_ANYONECANPAY) != 0;

        uint32_t noflag =
            sigHashType & ~(SIGHASH_FORKID | SIGHASH_ANYONECANPAY);
        bool isDefined = (noflag != 0) && (noflag <= SIGHASH_SINGLE);

        const SigHashType tbase(rawType);

        // Check deserialization.
        CheckSigHashType(tbase,
                         BaseSigHashType(baseType),
                         isDefined,
                         hasForkId,
                         hasAnyoneCanPay);

        // Check raw value.
        BOOST_CHECK_EQUAL(tbase.getRawSigHashType(), rawType);

        // Check serialization/deserialization.
        uint32_t unserializedOutput;
        (CDataStream(SER_DISK, 0) << tbase) >> unserializedOutput;
        BOOST_CHECK_EQUAL(unserializedOutput, rawType);
    }
}

BOOST_AUTO_TEST_SUITE_END()
