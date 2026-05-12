#pragma once

#include "data/BackendInterface.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/NFTOffersCommon.hpp"

#include <memory>

namespace rpc {

/**
 * @brief The nft_sell_offers method returns a list of sell offers for a given NFToken object.
 *
 * For more details see: https://xrpl.org/nft_sell_offers.html
 */
class NFTSellOffersHandler : public NFTOffersHandlerBase {
public:
    /**
     * @brief Construct a new NFTSellOffersHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    NFTSellOffersHandler(std::shared_ptr<BackendInterface> sharedPtrBackend)
        : NFTOffersHandlerBase(sharedPtrBackend)
    {
    }

    /**
     * @brief Process the NFTSellOffers command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    [[nodiscard]] Result
    process(Input const& input, Context const& ctx) const;
};

}  // namespace rpc
