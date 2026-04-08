#include "rpc/handlers/Random.hpp"

#include "rpc/JS.hpp"
#include "rpc/common/Types.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/beast/utility/rngfill.h>
#include <xrpl/crypto/csprng.h>
#include <xrpl/protocol/jss.h>

namespace rpc {

RandomHandler::Result
RandomHandler::process([[maybe_unused]] Context const& ctx)
{
    ripple::uint256 rand;
    beast::rngfill(rand.begin(), ripple::uint256::size(), ripple::crypto_prng());

    return Output{ripple::strHex(rand)};
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, RandomHandler::Output const& output)
{
    jv = {
        {JS(random), output.random},
    };
}

}  // namespace rpc
