#include "rpc/handlers/NFTSellOffers.hpp"

#include "rpc/common/Types.hpp"

#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>

using namespace ripple;

namespace rpc {

NFTSellOffersHandler::Result
NFTSellOffersHandler::process(NFTSellOffersHandler::Input const& input, Context const& ctx) const
{
    auto const tokenID = uint256{input.nftID.c_str()};
    auto const directory = keylet::nft_sells(tokenID);

    return iterateOfferDirectory(input, tokenID, directory, ctx.yield);
}

}  // namespace rpc
