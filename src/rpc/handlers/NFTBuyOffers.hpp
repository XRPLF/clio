//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

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
    NFTBuyOffersHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend)
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
    process(Input input, Context const& ctx) const;
};
}  // namespace rpc
