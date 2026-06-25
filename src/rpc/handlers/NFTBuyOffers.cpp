#include "rpc/handlers/NFTBuyOffers.hpp"

#include "rpc/common/Types.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>

using namespace xrpl;

namespace rpc {

NFTBuyOffersHandler::Result
NFTBuyOffersHandler::process(NFTBuyOffersHandler::Input const& input, Context const& ctx) const
{
    auto const tokenID = uint256{input.nftID.c_str()};
    auto const directory = keylet::nftBuys(tokenID);

    return iterateOfferDirectory(input, tokenID, directory, ctx.yield);
}
}  // namespace rpc
