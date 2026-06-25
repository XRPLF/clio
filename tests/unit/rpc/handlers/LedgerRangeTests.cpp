#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/LedgerRange.hpp"
#include "util/HandlerBaseTestFixture.hpp"

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

using namespace rpc;
using namespace data;
using namespace testing;

namespace {

constexpr auto kRangeMin = 10;
constexpr auto kRangeMax = 30;

}  // namespace

class RPCLedgerRangeTest : public HandlerBaseTest {};

TEST_F(RPCLedgerRangeTest, LedgerRangeMinMaxSame)
{
    runSpawn([this](auto yield) {
        backend_->updateRange(kRangeMin);

        auto const handler = AnyHandler{LedgerRangeHandler{backend_}};
        auto const req = boost::json::parse("{}");
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        auto const json = output.result.value();
        EXPECT_EQ(json.at("ledger_index_min").as_uint64(), kRangeMin);
        EXPECT_EQ(json.at("ledger_index_max").as_uint64(), kRangeMin);
    });
}

TEST_F(RPCLedgerRangeTest, LedgerRangeFullySet)
{
    runSpawn([this](auto yield) {
        backend_->setRange(kRangeMin, kRangeMax);

        auto const handler = AnyHandler{LedgerRangeHandler{backend_}};
        auto const req = boost::json::parse("{}");
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        auto const json = output.result.value();
        EXPECT_EQ(json.at("ledger_index_min").as_uint64(), kRangeMin);
        EXPECT_EQ(json.at("ledger_index_max").as_uint64(), kRangeMax);
    });
}
