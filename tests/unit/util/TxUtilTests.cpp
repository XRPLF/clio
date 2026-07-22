#include "util/JsonUtils.hpp"
#include "util/TxUtils.hpp"

#include <gtest/gtest.h>
#include <xrpl/protocol/TxFormats.h>

#include <algorithm>
#include <cstddef>
#include <iterator>

TEST(TxUtilTests, txTypesInLowercase)
{
    auto const& types = util::getTxTypesInLowercase();
    ASSERT_TRUE(
        std::size_t(
            std::distance(
                xrpl::TxFormats::getInstance().begin(), xrpl::TxFormats::getInstance().end()
            )
        ) == types.size()
    );

    std::for_each(
        xrpl::TxFormats::getInstance().begin(),
        xrpl::TxFormats::getInstance().end(),
        [&](auto const& pair) {
            EXPECT_TRUE(types.find(util::toLower(pair.getName())) != types.end());
        }
    );
}
