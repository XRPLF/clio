#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/LedgerIndex.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

constexpr auto kRangeMin = 10;
constexpr auto kRangeMax = 30;
constexpr auto kLedgerHash = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";

}  // namespace

using namespace rpc;
using namespace data;
namespace json = boost::json;
using namespace testing;

struct RPCLedgerIndexTest : HandlerBaseTestStrict {
    RPCLedgerIndexTest()
    {
        backend_->setRange(kRangeMin, kRangeMax);
    }
};

TEST_F(RPCLedgerIndexTest, DateStrNotValid)
{
    auto const handler = AnyHandler{LedgerIndexHandler{backend_}};
    auto const req = json::parse(R"JSON({"date": "not_a_number"})JSON");
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "Invalid parameters.");
    });
}

TEST_F(RPCLedgerIndexTest, NoDateGiven)
{
    auto const ledgerHeader = createLedgerHeader(kLedgerHash, kRangeMax, 5);
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMax, _)).WillOnce(Return(ledgerHeader));

    auto const handler = AnyHandler{LedgerIndexHandler{backend_}};
    auto const req = json::parse(R"JSON({})JSON");
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ledger_index").as_uint64(), kRangeMax);
        EXPECT_EQ(output.result->at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_TRUE(output.result->as_object().contains("closed"));
    });
}

TEST_F(RPCLedgerIndexTest, EarlierThanMinLedger)
{
    auto const handler = AnyHandler{LedgerIndexHandler{backend_}};
    auto const req = json::parse(R"JSON({"date": "2024-06-25T12:23:05Z"})JSON");
    auto const ledgerHeader = createLedgerHeaderWithUnixTime(
        kLedgerHash, kRangeMin, 1719318190
    );  //"2024-06-25T12:23:10Z"
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMin, _)).WillOnce(Return(ledgerHeader));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
    });
}

TEST_F(RPCLedgerIndexTest, ChangeTimeZone)
{
    // Note: setenv/unsetenv are included with <cstdlib> but misc-include-cleaner still angry
    setenv("TZ", "EST+5", 1);  // NOLINT(misc-include-cleaner)
    auto const handler = AnyHandler{LedgerIndexHandler{backend_}};
    auto const req = json::parse(R"JSON({"date": "2024-06-25T12:23:05Z"})JSON");
    auto const ledgerHeader = createLedgerHeaderWithUnixTime(
        kLedgerHash, kRangeMin, 1719318190
    );  //"2024-06-25T12:23:10Z"
    EXPECT_CALL(*backend_, fetchLedgerBySequence(kRangeMin, _)).WillOnce(Return(ledgerHeader));
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
    });
    unsetenv("TZ");  // NOLINT(misc-include-cleaner)
}

struct LedgerIndexTestsCaseBundle {
    std::string testName;
    std::string json;
    std::uint32_t expectedLedgerIndex;
    std::string closeTimeIso;
};

class LedgerIndexTests : public RPCLedgerIndexTest,
                         public WithParamInterface<LedgerIndexTestsCaseBundle> {
public:
    static auto
    generateTestValuesForParametersTest()
    {
        // start from 2024-06-25T12:23:10Z to 2024-06-25T12:23:50Z with step 2
        return std::vector<LedgerIndexTestsCaseBundle>{
            {.testName = "LaterThanMaxLedger",
             .json = R"JSON({"date": "2024-06-25T12:23:55Z"})JSON",
             .expectedLedgerIndex = kRangeMax,
             .closeTimeIso = "2024-06-25T12:23:50Z"},
            {.testName = "GreaterThanMinLedger",
             .json = R"JSON({"date": "2024-06-25T12:23:11Z"})JSON",
             .expectedLedgerIndex = kRangeMin,
             .closeTimeIso = "2024-06-25T12:23:10Z"},
            {.testName = "IsMinLedger",
             .json = R"JSON({"date": "2024-06-25T12:23:10Z"})JSON",
             .expectedLedgerIndex = kRangeMin,
             .closeTimeIso = "2024-06-25T12:23:10Z"},
            {.testName = "IsMaxLedger",
             .json = R"JSON({"date": "2024-06-25T12:23:50Z"})JSON",
             .expectedLedgerIndex = kRangeMax,
             .closeTimeIso = "2024-06-25T12:23:50Z"},
            {.testName = "IsMidLedger",
             .json = R"JSON({"date": "2024-06-25T12:23:30Z"})JSON",
             .expectedLedgerIndex = 20,
             .closeTimeIso = "2024-06-25T12:23:30Z"},
            {.testName = "BetweenLedgers",
             .json = R"JSON({"date": "2024-06-25T12:23:29Z"})JSON",
             .expectedLedgerIndex = 19,
             .closeTimeIso = "2024-06-25T12:23:28Z"}
        };
    }
};

INSTANTIATE_TEST_CASE_P(
    RPCLedgerIndexTestsGroup,
    LedgerIndexTests,
    ValuesIn(LedgerIndexTests::generateTestValuesForParametersTest()),
    tests::util::kNameGenerator
);

TEST_P(LedgerIndexTests, SearchFromLedgerRange)
{
    auto const testBundle = GetParam();
    auto const jv = json::parse(testBundle.json).as_object();

    // start from 1719318190 , which is the unix time for 2024-06-25T12:23:10Z to
    // 2024-06-25T12:23:50Z with step 2
    for (uint32_t i = kRangeMin; i <= kRangeMax; i++) {
        auto const ledgerHeader =
            createLedgerHeaderWithUnixTime(kLedgerHash, i, 1719318190 + (2 * (i - kRangeMin)));
        auto const exactNumberOfCalls = i == kRangeMin ? Exactly(3) : Exactly(2);
        EXPECT_CALL(*backend_, fetchLedgerBySequence(i, _))
            .Times(i == testBundle.expectedLedgerIndex ? exactNumberOfCalls : AtMost(1))
            .WillRepeatedly(Return(ledgerHeader));
    }

    auto const handler = AnyHandler{LedgerIndexHandler{backend_}};
    auto const req = json::parse(testBundle.json);
    runSpawn([&](auto yield) {
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("ledger_index").as_uint64(), testBundle.expectedLedgerIndex);
        EXPECT_EQ(output.result->at("ledger_hash").as_string(), kLedgerHash);
        EXPECT_EQ(output.result->at("closed").as_string(), testBundle.closeTimeIso);
    });
}
