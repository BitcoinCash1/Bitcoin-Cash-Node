// Copyright (c) 2018 The Bitcoin Core developers
// Copyright (c) 2019-2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/policy.h>
#include <script/descriptor.h>
#include <script/sign.h>
#include <script/standard.h>
#include <util/strencodings.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

namespace {

void CheckUnparsable(const std::string &prv, const std::string &pub) {
    FlatSigningProvider keys_priv, keys_pub;
    auto parse_priv = Parse(prv, keys_priv);
    auto parse_pub = Parse(pub, keys_pub);
    BOOST_CHECK(!parse_priv);
    BOOST_CHECK(!parse_pub);
}

constexpr int DEFAULT = 0;
// Expected to be ranged descriptor
constexpr int RANGE = 1;
// Derivation needs access to private keys
constexpr int HARDENED = 2;
// This descriptor is not expected to be solvable
constexpr int UNSOLVABLE = 4;
// We can sign with this descriptor (this is not true when actual BIP32
// derivation is used, as that's not integrated in our signing code)
constexpr int SIGNABLE = 8;

std::string MaybeUseHInsteadOfApostrophy(std::string ret) {
    if (InsecureRandBool()) {
        while (true) {
            auto it = ret.find("'");
            if (it != std::string::npos) {
                ret[it] = 'h';
            } else {
                break;
            }
        }
    }
    return ret;
}

const std::set<std::vector<uint32_t>> ONLY_EMPTY{{}};

void Check(const std::string &prv, const std::string &pub, int flags,
           const std::vector<std::vector<std::string>> &scripts,
           const std::set<std::vector<uint32_t>> &paths = ONLY_EMPTY) {
    FlatSigningProvider keys_priv, keys_pub;
    std::set<std::vector<uint32_t>> left_paths = paths;

    // Check that parsing succeeds.
    auto parse_priv = Parse(MaybeUseHInsteadOfApostrophy(prv), keys_priv);
    auto parse_pub = Parse(MaybeUseHInsteadOfApostrophy(pub), keys_pub);
    BOOST_CHECK(parse_priv);
    BOOST_CHECK(parse_pub);

    // Check private keys are extracted from the private version but not the
    // public one.
    BOOST_CHECK(keys_priv.keys.size());
    BOOST_CHECK(!keys_pub.keys.size());

    // Check that both versions serialize back to the public version.
    std::string pub1 = parse_priv->ToString();
    std::string pub2 = parse_pub->ToString();
    BOOST_CHECK_EQUAL(pub, pub1);
    BOOST_CHECK_EQUAL(pub, pub2);

    // Check that both can be serialized with private key back to the private
    // version, but not without private key.
    std::string prv1, prv2;
    BOOST_CHECK(parse_priv->ToPrivateString(keys_priv, prv1));
    BOOST_CHECK_EQUAL(prv, prv1);
    BOOST_CHECK(!parse_priv->ToPrivateString(keys_pub, prv1));
    BOOST_CHECK(parse_pub->ToPrivateString(keys_priv, prv1));
    BOOST_CHECK_EQUAL(prv, prv1);
    BOOST_CHECK(!parse_pub->ToPrivateString(keys_pub, prv1));

    // Check whether IsRange on both returns the expected result
    BOOST_CHECK_EQUAL(parse_pub->IsRange(), (flags & RANGE) != 0);
    BOOST_CHECK_EQUAL(parse_priv->IsRange(), (flags & RANGE) != 0);

    // Is not ranged descriptor, only a single result is expected.
    if (!(flags & RANGE)) {
        assert(scripts.size() == 1);
    }

    auto const null_context = std::nullopt;

    size_t max = (flags & RANGE) ? scripts.size() : 3;
    for (size_t i = 0; i < max; ++i) {
        const auto &ref = scripts[(flags & RANGE) ? i : 0];
        for (int t = 0; t < 2; ++t) {
            const FlatSigningProvider &key_provider =
                (flags & HARDENED) ? keys_priv : keys_pub;
            FlatSigningProvider script_provider;
            std::vector<CScript> spks;
            BOOST_CHECK((t ? parse_priv : parse_pub)
                            ->Expand(i, key_provider, spks, script_provider));
            BOOST_CHECK_EQUAL(spks.size(), ref.size());
            for (size_t n = 0; n < spks.size(); ++n) {
                BOOST_CHECK_EQUAL(ref[n], HexStr(spks[n]));

                BOOST_CHECK_EQUAL(
                    IsSolvable(Merge(key_provider, script_provider), spks[n], STANDARD_SCRIPT_VERIFY_FLAGS),
                    (flags & UNSOLVABLE) == 0);

                if (flags & SIGNABLE) {
                    CMutableTransaction spend;
                    spend.vin.resize(1);
                    spend.vout.resize(1);
                    BOOST_CHECK_MESSAGE(
                        SignSignature(Merge(keys_priv, script_provider),
                                      spks[n], spend, 0, CTxOut{1 * COIN, spks[n]},
                                      SigHashType().withFork(), STANDARD_SCRIPT_VERIFY_FLAGS, null_context),
                        prv);
                }
            }
            // Test whether the observed key path is present in the 'paths'
            // variable (which contains expected, unobserved paths), and then
            // remove it from that set.
            for (const auto &origin : script_provider.origins) {
                BOOST_CHECK_MESSAGE(paths.count(origin.second.path),
                                    "Unexpected key path: " + prv);
                left_paths.erase(origin.second.path);
            }
        }
    }
    // Verify no expected paths remain that were not observed.
    BOOST_CHECK_MESSAGE(left_paths.empty(),
                        "Not all expected key paths found: " + prv);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(descriptor_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(descriptor_test) {
    // Basic single-key compressed
    Check("combo(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)",
          "combo("
          "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)",
          SIGNABLE,
          {{"2103a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5"
            "bdac",
            "76a9149a1c78a507689f6f54b847ad1cef1e614ee23f1e88ac"}});
    Check("pk(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)",
          "pk("
          "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)",
          SIGNABLE,
          {{"2103a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5"
            "bdac"}});
    Check("pkh([deadbeef/1/2'/3/4']"
          "L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)",
          "pkh([deadbeef/1/2'/3/4']"
          "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)",
          SIGNABLE, {{"76a9149a1c78a507689f6f54b847ad1cef1e614ee23f1e88ac"}},
          {{1, 0x80000002UL, 3, 0x80000004UL}});

    // Basic single-key uncompressed
    Check(
        "combo(5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss)",
        "combo("
        "04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b8d"
        "ec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235)",
        SIGNABLE,
        {{"4104a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd"
          "5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235ac",
          "76a914b5bd079c4d57cc7fc28ecf8213a6b791625b818388ac"}});
    Check("pk(5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss)",
          "pk("
          "04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b"
          "8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235)",
          SIGNABLE,
          {{"4104a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5"
            "bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235"
            "ac"}});
    Check("pkh(5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss)",
          "pkh("
          "04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b"
          "8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235)",
          SIGNABLE, {{"76a914b5bd079c4d57cc7fc28ecf8213a6b791625b818388ac"}});

    // Some unconventional single-key constructions
    Check(
        "sh(pk(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1))",
        "sh(pk("
        "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd))",
        SIGNABLE, {{"a9141857af51a5e516552b3086430fd8ce55f7c1a52487"}});
    Check(
        "sh(pkh(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1))",
        "sh(pkh("
        "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd))",
        SIGNABLE, {{"a9141a31ad23bf49c247dd531a623c2ef57da3c400c587"}});

    // Versions with BIP32 derivations
    Check("combo([01234567]"
          "xprvA1RpRA33e1JQ7ifknakTFpgNXPmW2YvmhqLQYMmrj4xJXXWYpDPS3xz7iAxn8L39"
          "njGVyuoseXzU6rcxFLJ8HFsTjSyQbLYnMpCqE2VbFWc)",
          "combo([01234567]"
          "xpub6ERApfZwUNrhLCkDtcHTcxd75RbzS1ed54G1LkBUHQVHQKqhMkhgbmJbZRkrgZw4"
          "koxb5JaHWkY4ALHY2grBGRjaDMzQLcgJvLJuZZvRcEL)",
          SIGNABLE,
          {{"2102d2b36900396c9282fa14628566582f206a5dd0bcc8d5e892611806cafb0301"
            "f0ac",
            "76a91431a507b815593dfc51ffc7245ae7e5aee304246e88ac"}});
    Check("pk("
          "xprv9uPDJpEQgRQfDcW7BkF7eTya6RPxXeJCqCJGHuCJ4GiRVLzkTXBAJMu2qaMWPrS7"
          "AANYqdq6vcBcBUdJCVVFceUvJFjaPdGZ2y9WACViL4L/0)",
          "pk("
          "xpub68NZiKmJWnxxS6aaHmn81bvJeTESw724CRDs6HbuccFQN9Ku14VQrADWgqbhhTHB"
          "aohPX4CjNLf9fq9MYo6oDaPPLPxSb7gwQN3ih19Zm4Y/0)",
          DEFAULT,
          {{"210379e45b3cf75f9c5f9befd8e9506fb962f6a9d185ac87001ec44a8d3df8d4a9"
            "e3ac"}},
          {{0}});
    Check("pkh("
          "xprv9s21ZrQH143K31xYSDQpPDxsXRTUcvj2iNHm5NUtrGiGG5e2DtALGdso3pGz6ssr"
          "dK4PFmM8NSpSBHNqPqm55Qn3LqFtT2emdEXVYsCzC2U/2147483647'/0)",
          "pkh("
          "xpub661MyMwAqRbcFW31YEwpkMuc5THy2PSt5bDMsktWQcFF8syAmRUapSCGu8ED9W6o"
          "DMSgv6Zz8idoc4a6mr8BDzTJY47LJhkJ8UB7WEGuduB/2147483647'/0)",
          HARDENED, {{"76a914ebdc90806a9c4356c1c88e42216611e1cb4c1c1788ac"}},
          {{0xFFFFFFFFUL, 0}});
    Check("combo("
          "xprvA2JDeKCSNNZky6uBCviVfJSKyQ1mDYahRjijr5idH2WwLsEd4Hsb2Tyh8RfQMuPh"
          "7f7RtyzTtdrbdqqsunu5Mm3wDvUAKRHSC34sJ7in334/*)",
          "combo("
          "xpub6FHa3pjLCk84BayeJxFW2SP4XRrFd1JYnxeLeU8EqN3vDfZmbqBqaGJAyiLjTAwm"
          "6ZLRQUMv1ZACTj37sR62cfN7fe5JnJ7dh8zL4fiyLHV/*)",
          RANGE,
          {{"2102df12b7035bdac8e3bab862a3a83d06ea6b17b6753d52edecba9be46f5d09e0"
            "76ac",
            "76a914f90e3178ca25f2c808dc76624032d352fdbdfaf288ac"},
           {"21032869a233c9adff9a994e4966e5b821fd5bac066da6c3112488dc52383b4a98"
            "ecac",
            "76a914a8409d1b6dfb1ed2a3e8aa5e0ef2ff26b15b75b788ac"}},
          {{0}, {1}});
    // BIP 32 path element overflow
    CheckUnparsable(
        "pkh("
        "xprv9s21ZrQH143K31xYSDQpPDxsXRTUcvj2iNHm5NUtrGiGG5e2DtALGdso3pGz6ssrdK"
        "4PFmM8NSpSBHNqPqm55Qn3LqFtT2emdEXVYsCzC2U/2147483648)",
        "pkh("
        "xpub661MyMwAqRbcFW31YEwpkMuc5THy2PSt5bDMsktWQcFF8syAmRUapSCGu8ED9W6oDM"
        "Sgv6Zz8idoc4a6mr8BDzTJY47LJhkJ8UB7WEGuduB/2147483648)");

    // Multisig constructions
    Check("multi(1,L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1,"
          "5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss)",
          "multi(1,"
          "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd,"
          "04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b"
          "8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235)",
          SIGNABLE,
          {{"512103a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540"
            "c5bd4104a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c5"
            "40c5bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abe"
            "a23552ae"}});
    Check("sh(multi(2,[00000000/111'/222]"
          "xprvA1RpRA33e1JQ7ifknakTFpgNXPmW2YvmhqLQYMmrj4xJXXWYpDPS3xz7iAxn8L39"
          "njGVyuoseXzU6rcxFLJ8HFsTjSyQbLYnMpCqE2VbFWc,"
          "xprv9uPDJpEQgRQfDcW7BkF7eTya6RPxXeJCqCJGHuCJ4GiRVLzkTXBAJMu2qaMWPrS7"
          "AANYqdq6vcBcBUdJCVVFceUvJFjaPdGZ2y9WACViL4L/0))",
          "sh(multi(2,[00000000/111'/222]"
          "xpub6ERApfZwUNrhLCkDtcHTcxd75RbzS1ed54G1LkBUHQVHQKqhMkhgbmJbZRkrgZw4"
          "koxb5JaHWkY4ALHY2grBGRjaDMzQLcgJvLJuZZvRcEL,"
          "xpub68NZiKmJWnxxS6aaHmn81bvJeTESw724CRDs6HbuccFQN9Ku14VQrADWgqbhhTHB"
          "aohPX4CjNLf9fq9MYo6oDaPPLPxSb7gwQN3ih19Zm4Y/0))",
          DEFAULT, {{"a91445a9a622a8b0a1269944be477640eedc447bbd8487"}},
          {{0x8000006FUL, 222}, {0}});
    // P2SH does not fit 16 compressed pubkeys in a redeemscript
    CheckUnparsable(
        "sh(multi(16,"
        "KzoAz5CanayRKex3fSLQ2BwJpN7U52gZvxMyk78nDMHuqrUxuSJy,"
        "KwGNz6YCCQtYvFzMtrC6D3tKTKdBBboMrLTsjr2NYVBwapCkn7Mr,"
        "KxogYhiNfwxuswvXV66eFyKcCpm7dZ7TqHVqujHAVUjJxyivxQ9X,"
        "L2BUNduTSyZwZjwNHynQTF14mv2uz2NRq5n5sYWTb4FkkmqgEE9f,"
        "L1okJGHGn1kFjdXHKxXjwVVtmCMR2JA5QsbKCSpSb7ReQjezKeoD,"
        "KxDCNSST75HFPaW5QKpzHtAyaCQC7p9Vo3FYfi2u4dXD1vgMiboK,"
        "L5edQjFtnkcf5UWURn6UuuoFrabgDQUHdheKCziwN42aLwS3KizU,"
        "KzF8UWFcEC7BYTq8Go1xVimMkDmyNYVmXV5PV7RuDicvAocoPB8i,"
        "L3nHUboKG2w4VSJ5jYZ5CBM97oeK6YuKvfZxrefdShECcjEYKMWZ,"
        "KyjHo36dWkYhimKmVVmQTq3gERv3pnqA4xFCpvUgbGDJad7eS8WE,"
        "KwsfyHKRUTZPQtysN7M3tZ4GXTnuov5XRgjdF2XCG8faAPmFruRF,"
        "KzCUbGhN9LJhdeFfL9zQgTJMjqxdBKEekRGZX24hXdgCNCijkkap,"
        "KzgpMBwwsDLwkaC5UrmBgCYaBD2WgZ7PBoGYXR8KT7gCA9UTN5a3,"
        "KyBXTPy4T7YG4q9tcAM3LkvfRpD1ybHMvcJ2ehaWXaSqeGUxEdkP,"
        "KzJDe9iwJRPtKP2F2AoN6zBgzS7uiuAwhWCfGdNeYJ3PC1HNJ8M8,"
        "L1xbHrxynrqLKkoYc4qtoQPx6uy5qYXR5ZDYVYBSRmCV5piU3JG9))",
        "sh(multi(16,"
        "03669b8afcec803a0d323e9a17f3ea8e68e8abe5a278020a929adbec52421adbd0,"
        "0260b2003c386519fc9eadf2b5cf124dd8eea4c4e68d5e154050a9346ea98ce600,"
        "0362a74e399c39ed5593852a30147f2959b56bb827dfa3e60e464b02ccf87dc5e8,"
        "0261345b53de74a4d721ef877c255429961b7e43714171ac06168d7e08c542a8b8,"
        "02da72e8b46901a65d4374fe6315538d8f368557dda3a1dcf9ea903f3afe7314c8,"
        "0318c82dd0b53fd3a932d16e0ba9e278fcc937c582d5781be626ff16e201f72286,"
        "0297ccef1ef99f9d73dec9ad37476ddb232f1238aff877af19e72ba04493361009,"
        "02e502cfd5c3f972fe9a3e2a18827820638f96b6f347e54d63deb839011fd5765d,"
        "03e687710f0e3ebe81c1037074da939d409c0025f17eb86adb9427d28f0f7ae0e9,"
        "02c04d3a5274952acdbc76987f3184b346a483d43be40874624b29e3692c1df5af,"
        "02ed06e0f418b5b43a7ec01d1d7d27290fa15f75771cb69b642a51471c29c84acd,"
        "036d46073cbb9ffee90473f3da429abc8de7f8751199da44485682a989a4bebb24,"
        "02f5d1ff7c9029a80a4e36b9a5497027ef7f3e73384a4a94fbfe7c4e9164eec8bc,"
        "02e41deffd1b7cce11cde209a781adcffdabd1b91c0ba0375857a2bfd9302419f3,"
        "02d76625f7956a7fc505ab02556c23ee72d832f1bac391bcd2d3abce5710a13d06,"
        "0399eb0a5487515802dc14544cf10b3666623762fbed2ec38a3975716e2c29c232))");

    // Check for invalid nesting of structures

    // P2SH needs a script, not a key
    CheckUnparsable(
        "sh(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)",
        "sh("
        "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)");
    // Old must be top level
    CheckUnparsable(
        "sh(combo("
        "L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1))",
        "sh(combo("
        "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd))");
    // Cannot embed P2SH inside P2SH
    CheckUnparsable(
        "sh(sh(pk(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)))",
        "sh(sh(pk("
        "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd))"
        ")");
}

BOOST_AUTO_TEST_SUITE_END()
