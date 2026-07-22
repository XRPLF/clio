#include "util/JsonUtils.hpp"
#include "util/NameGenerator.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

TEST(JsonUtils, RemoveSecrets)
{
    auto json = boost::json::parse(R"JSON({
        "secret": "snoopy",
        "seed": "woodstock",
        "seed_hex": "charlie",
        "passphrase": "lucy"
    })JSON")
                    .as_object();

    auto json2 = util::removeSecret(json);
    EXPECT_EQ(json2.at("secret").as_string(), "*");
    EXPECT_EQ(json2.at("seed").as_string(), "*");
    EXPECT_EQ(json2.at("seed_hex").as_string(), "*");
    EXPECT_EQ(json2.at("passphrase").as_string(), "*");

    json = boost::json::parse(R"JSON({
        "params": [
            {
                "secret": "snoopy",
                "seed": "woodstock",
                "seed_hex": "charlie",
                "passphrase": "lucy"
            }
        ]
    })JSON")
               .as_object();

    json2 = util::removeSecret(json);
    EXPECT_TRUE(json2.contains("params"));
    EXPECT_TRUE(json2.at("params").is_array());
    EXPECT_TRUE(!json2.at("params").as_array().empty());
    json2 = json2.at("params").as_array()[0].as_object();
    EXPECT_EQ(json2.at("secret").as_string(), "*");
    EXPECT_EQ(json2.at("seed").as_string(), "*");
    EXPECT_EQ(json2.at("seed_hex").as_string(), "*");
    EXPECT_EQ(json2.at("passphrase").as_string(), "*");
}

TEST(JsonUtils, integralValueAs)
{
    auto const expectedResultUint64 =
        static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) + 1u;
    auto const uint64Json = boost::json::value(expectedResultUint64);
    EXPECT_EQ(util::integralValueAs<int32_t>(uint64Json), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(util::integralValueAs<uint32_t>(uint64Json), expectedResultUint64);
    EXPECT_EQ(util::integralValueAs<int64_t>(uint64Json), expectedResultUint64);
    EXPECT_EQ(util::integralValueAs<uint64_t>(uint64Json), expectedResultUint64);

    auto const expectedResultInt64 = static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1u;
    auto const int64Json = boost::json::value(expectedResultInt64);
    EXPECT_EQ(util::integralValueAs<int32_t>(int64Json), std::numeric_limits<int32_t>::min());
    EXPECT_EQ(util::integralValueAs<uint32_t>(int64Json), expectedResultInt64);
    EXPECT_EQ(util::integralValueAs<int64_t>(int64Json), expectedResultInt64);
    EXPECT_EQ(util::integralValueAs<uint64_t>(int64Json), expectedResultInt64);

    auto const doubleJson = boost::json::value(3.14);
    EXPECT_THROW(util::integralValueAs<int>(doubleJson), std::logic_error);

    auto const stringJson = boost::json::value("not a number");
    EXPECT_THROW(util::integralValueAs<int>(stringJson), std::logic_error);
}

TEST(JsonUtils, tryIntegralValueAs)
{
    auto const expectedResultUint64 =
        static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) + 1u;
    auto const uint64Json = boost::json::value(expectedResultUint64);

    auto const expectedResultInt64 = static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + 1u;
    auto const int64Json = boost::json::value(expectedResultInt64);

    auto checkHasValue = [&](boost::json::value const& jv, auto const& expectedValue) {
        using T = std::remove_cvref_t<decltype(expectedValue)>;
        auto const res = util::tryIntegralValueAs<T>(jv);
        ASSERT_TRUE(res.has_value());
        EXPECT_EQ(res.value(), expectedValue);
    };

    auto checkError = [&](boost::json::value const& jv) {
        auto res = util::tryIntegralValueAs<int>(jv);
        EXPECT_FALSE(res.has_value());
        EXPECT_EQ(res.error(), "Value neither uint64 nor int64");
    };

    // checks for uint64Json
    checkHasValue(uint64Json, std::numeric_limits<int32_t>::min());
    checkHasValue(uint64Json, static_cast<uint32_t>(expectedResultUint64));
    checkHasValue(uint64Json, static_cast<int64_t>(expectedResultUint64));
    checkHasValue(uint64Json, expectedResultUint64);

    // checks for int64Json
    checkHasValue(int64Json, std::numeric_limits<int32_t>::min());
    checkHasValue(int64Json, static_cast<uint32_t>(expectedResultInt64));
    checkHasValue(int64Json, expectedResultInt64);
    checkHasValue(int64Json, static_cast<uint64_t>(expectedResultInt64));

    // non-integral inputs
    checkError(boost::json::value());
    checkError(boost::json::value(false));
    checkError(boost::json::value(3.14));
    checkError(boost::json::value("not a number"));
}

struct GetLedgerIndexParameterTestBundle {
    std::string testName;
    boost::json::value jv;
    std::expected<uint32_t, std::string> expectedResult;
};

// parameterized test cases for parameters check
struct GetLedgerIndexParameterTest : ::testing::TestWithParam<GetLedgerIndexParameterTestBundle> {};

INSTANTIATE_TEST_CASE_P(
    JsonUtils,
    GetLedgerIndexParameterTest,
    testing::Values(
        GetLedgerIndexParameterTestBundle{
            .testName = "EmptyValue",
            .jv = boost::json::value(),
            .expectedResult = std::unexpected{"Value neither uint64 nor int64"}
        },
        GetLedgerIndexParameterTestBundle{
            .testName = "BoolValue",
            .jv = boost::json::value(false),
            .expectedResult = std::unexpected{"Value neither uint64 nor int64"}
        },
        GetLedgerIndexParameterTestBundle{
            .testName = "NumberValue",
            .jv = boost::json::value(123),
            .expectedResult = 123u
        },
        GetLedgerIndexParameterTestBundle{
            .testName = "StringNumberValue",
            .jv = boost::json::value("123"),
            .expectedResult = 123u
        },
        GetLedgerIndexParameterTestBundle{
            .testName = "StringNumberWithPlusSignValue",
            .jv = boost::json::value("+123"),
            .expectedResult = 123u
        },
        GetLedgerIndexParameterTestBundle{
            .testName = "StringEmptyValue",
            .jv = boost::json::value(""),
            .expectedResult = std::unexpected{"Invalid ledger index string"}
        },
        GetLedgerIndexParameterTestBundle{
            .testName = "StringWithLeadingCharsValue",
            .jv = boost::json::value("123invalid"),
            .expectedResult = std::unexpected{"Invalid ledger index string"}
        },
        GetLedgerIndexParameterTestBundle{
            .testName = "StringWithTrailingCharsValue",
            .jv = boost::json::value("invalid123"),
            .expectedResult = std::unexpected{"Invalid ledger index string"}
        },
        GetLedgerIndexParameterTestBundle{
            .testName = "StringWithLeadingAndTrailingCharsValue",
            .jv = boost::json::value("123invalid123"),
            .expectedResult = std::unexpected{"Invalid ledger index string"}
        },
        GetLedgerIndexParameterTestBundle{
            .testName = "ValidatedStringValue",
            .jv = boost::json::value("validated"),
            .expectedResult = std::unexpected{"'validated' ledger index is requested"}
        }
    ),
    tests::util::kNameGenerator
);

TEST_P(GetLedgerIndexParameterTest, getLedgerIndexParams)
{
    auto const& testBundle = GetParam();
    auto const ledgerIndex = util::getLedgerIndex(testBundle.jv);

    if (testBundle.expectedResult.has_value()) {
        EXPECT_TRUE(ledgerIndex.has_value());
        EXPECT_EQ(ledgerIndex.value(), testBundle.expectedResult.value());
    } else {
        EXPECT_FALSE(ledgerIndex.has_value());
        EXPECT_EQ(ledgerIndex.error(), testBundle.expectedResult.error());
    }
}
