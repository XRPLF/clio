#include "rpc/handlers/NFTBuyOffers.hpp"

#include "rpc/common/Types.hpp"

#include <xrpl/protocol/Indexes.h>

using namespace xrpl;

namespace rpc {

NFTBuyOffersHandler::Result
NFTBuyOffersHandler::process(NFTBuyOffersHandler::Input const& input, Context const& ctx) const
{
    auto const& tokenID = input.nftID;
    auto const directory = keylet::nftBuys(tokenID);

    return iterateOfferDirectory(input, tokenID, directory, ctx.yield);
}
}  // namespace rpc
