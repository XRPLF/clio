#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/LedgerRange.hpp"
#include "util/HandlerBaseTestFixture.hpp"

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

using namespace rpc;
using namespace data;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kRANGE_MIN = 10;
constexpr auto kRANGE_MAX = 30;

}  // namespace

class RPCLedgerRangeTest : public HandlerBaseTest {};

TEST_F(RPCLedgerRangeTest, LedgerRangeMinMaxSame)
{
    runSpawn([this](auto yield) {
        backend_->updateRange(kRANGE_MIN);

        auto const handler = AnyHandler{LedgerRangeHandler{backend_}};
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        auto const json = output.result.value();
        EXPECT_EQ(json.at("ledger_index_min").as_uint64(), kRANGE_MIN);
        EXPECT_EQ(json.at("ledger_index_max").as_uint64(), kRANGE_MIN);
    });
}

TEST_F(RPCLedgerRangeTest, LedgerRangeFullySet)
{
    runSpawn([this](auto yield) {
        backend_->setRange(kRANGE_MIN, kRANGE_MAX);

        auto const handler = AnyHandler{LedgerRangeHandler{backend_}};
        auto const req = json::parse("{}");
        auto const output = handler.process(req, Context{yield});
        ASSERT_TRUE(output);
        auto const json = output.result.value();
        EXPECT_EQ(json.at("ledger_index_min").as_uint64(), kRANGE_MIN);
        EXPECT_EQ(json.at("ledger_index_max").as_uint64(), kRANGE_MAX);
    });
}
