#include "rpc/Amendments.hpp"

#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>

using namespace rpc;

TEST(RPCAmendmentsTest, GenerateAmendmentId)
{
    // https://xrpl.org/known-amendments.html#disallowincoming refer to the published id
    EXPECT_EQ(
        ripple::uint256("47C3002ABA31628447E8E9A8B315FAA935CE30183F9A9B86845E469CA2CDC3DF"),
        Amendments::GetAmendmentId("DisallowIncoming")
    );
}
