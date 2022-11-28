// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2020-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/data/key_io_invalid.json.h>
#include <test/data/key_io_valid.json.h>

#include <chainparams.h>
#include <key.h>
#include <key_io.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/setup_common.h>
#include <test/jsonutil.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <univalue.h>

BOOST_FIXTURE_TEST_SUITE(key_io_tests, BasicTestingSetup)

// Goal: check that parsed keys match test payload
BOOST_AUTO_TEST_CASE(key_io_valid_parse) {
    UniValue::Array tests = read_json(std::string(
        json_tests::key_io_valid,
        json_tests::key_io_valid + sizeof(json_tests::key_io_valid)));
    CKey privkey;
    CTxDestination destination;
    SelectParams(CBaseChainParams::MAIN);

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        const UniValue& test = tests[idx];
        std::string strTest = UniValue::stringify(test);
        // Allow for extra stuff (useful for comments)
        if (test.size() < 3) {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();
        std::vector<uint8_t> exp_payload = ParseHex(test[1].get_str());
        const UniValue::Object &metadata = test[2].get_obj();
        bool isPrivkey = metadata["isPrivkey"].get_bool();
        SelectParams(metadata["chain"].get_str());
        const UniValue& tryCaseFlipUV = metadata["tryCaseFlip"];
        bool try_case_flip = tryCaseFlipUV.isNull() ? false : tryCaseFlipUV.get_bool();
        if (isPrivkey) {
            bool isCompressed = metadata["isCompressed"].get_bool();
            // Must be valid private key
            privkey = DecodeSecret(exp_base58string);
            BOOST_CHECK_MESSAGE(privkey.IsValid(), "!IsValid:" + strTest);
            BOOST_CHECK_MESSAGE(privkey.IsCompressed() == isCompressed,
                                "compressed mismatch:" + strTest);
            BOOST_CHECK_MESSAGE(privkey.size() == exp_payload.size() &&
                                    std::equal(privkey.begin(), privkey.end(),
                                               exp_payload.begin()),
                                "key mismatch:" + strTest);

            // Private key must be invalid public key
            destination = DecodeLegacyAddr(exp_base58string, Params());
            BOOST_CHECK_MESSAGE(!IsValidDestination(destination),
                                "IsValid privkey as pubkey:" + strTest);
        } else {
            // Must be valid public key
            destination = DecodeLegacyAddr(exp_base58string, Params());
            CScript script = GetScriptForDestination(destination);
            BOOST_CHECK_MESSAGE(IsValidDestination(destination),
                                "!IsValid:" + strTest);
            BOOST_CHECK_EQUAL(HexStr(script), HexStr(exp_payload));

            // Try flipped case version
            for (char &c : exp_base58string) {
                if (c >= 'a' && c <= 'z') {
                    c = (c - 'a') + 'A';
                } else if (c >= 'A' && c <= 'Z') {
                    c = (c - 'A') + 'a';
                }
            }
            destination = DecodeLegacyAddr(exp_base58string, Params());
            BOOST_CHECK_MESSAGE(IsValidDestination(destination) ==
                                    try_case_flip,
                                "!IsValid case flipped:" + strTest);
            if (IsValidDestination(destination)) {
                script = GetScriptForDestination(destination);
                BOOST_CHECK_EQUAL(HexStr(script), HexStr(exp_payload));
            }

            // Public key must be invalid private key
            privkey = DecodeSecret(exp_base58string);
            BOOST_CHECK_MESSAGE(!privkey.IsValid(),
                                "IsValid pubkey as privkey:" + strTest);
        }
    }
}

// Goal: check that generated keys match test vectors
BOOST_AUTO_TEST_CASE(key_io_valid_gen) {
    UniValue tests = read_json(std::string(
        json_tests::key_io_valid,
        json_tests::key_io_valid + sizeof(json_tests::key_io_valid)));

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        const UniValue& test = tests[idx];
        std::string strTest = UniValue::stringify(test);
        // Allow for extra stuff (useful for comments)
        if (test.size() < 3) {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();
        std::vector<uint8_t> exp_payload = ParseHex(test[1].get_str());
        const UniValue::Object &metadata = test[2].get_obj();
        bool isPrivkey = metadata["isPrivkey"].get_bool();
        SelectParams(metadata["chain"].get_str());
        if (isPrivkey) {
            bool isCompressed = metadata["isCompressed"].get_bool();
            CKey key;
            key.Set(exp_payload.begin(), exp_payload.end(), isCompressed);
            assert(key.IsValid());
            BOOST_CHECK_MESSAGE(EncodeSecret(key) == exp_base58string,
                                "result mismatch: " + strTest);
        } else {
            CTxDestination dest;
            CScript exp_script(exp_payload.begin(), exp_payload.end());
            BOOST_CHECK(ExtractDestination(exp_script, dest, SCRIPT_ENABLE_P2SH_32 /* allow p2sh32 */));
            std::string address = EncodeLegacyAddr(dest, Params());

            BOOST_CHECK_EQUAL(address, exp_base58string);
        }
    }

    SelectParams(CBaseChainParams::MAIN);
}

// Goal: check that base58 parsing code is robust against a variety of corrupted
// data
BOOST_AUTO_TEST_CASE(key_io_invalid) {
    // Negative testcases
    UniValue tests = read_json(std::string(
        json_tests::key_io_invalid,
        json_tests::key_io_invalid + sizeof(json_tests::key_io_invalid)));
    CKey privkey;
    CTxDestination destination;

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        const UniValue& test = tests[idx];
        std::string strTest = UniValue::stringify(test);
        // Allow for extra stuff (useful for comments)
        if (test.size() < 1) {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();

        // must be invalid as public and as private key
        for (const auto &chain :
             {CBaseChainParams::MAIN, CBaseChainParams::TESTNET, CBaseChainParams::TESTNET4, CBaseChainParams::SCALENET,
              CBaseChainParams::CHIPNET, CBaseChainParams::REGTEST}) {
            SelectParams(chain);
            destination = DecodeLegacyAddr(exp_base58string, Params());
            BOOST_CHECK_MESSAGE(!IsValidDestination(destination),
                                "IsValid pubkey in mainnet:" + strTest);
            privkey = DecodeSecret(exp_base58string);
            BOOST_CHECK_MESSAGE(!privkey.IsValid(),
                                "IsValid privkey in mainnet:" + strTest);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
