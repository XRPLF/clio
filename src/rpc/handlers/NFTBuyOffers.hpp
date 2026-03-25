#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/NFTOffersCommon.hpp"

#include <memory>

namespace rpc {

/**
 * @brief The nft_buy_offers method returns a list of buy offers for a given NFToken object.
 *
 * For more details see: https://xrpl.org/nft_buy_offers.html
 */
class NFTBuyOffersHandler : public NFTOffersHandlerBase {
public:
    /**
     * @brief Construct a new NFTBuyOffersHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    NFTBuyOffersHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : NFTOffersHandlerBase(sharedPtrBackend)
    {
    }

    /**
     * @brief Process the NFTBuyOffers command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    Result
    process(Input const& input, Context const& ctx) const;
};
}  // namespace rpc
