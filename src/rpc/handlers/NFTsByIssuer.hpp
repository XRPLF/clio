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
#include "rpc/JS.hpp"
#include "rpc/common/Modifiers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <ripple/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace rpc {

/**
 * @brief Handler for the `nfts_by_issuer` command
 */
class NFTsByIssuerHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    static auto constexpr LIMIT_MIN = 1;
    static auto constexpr LIMIT_MAX = 100;
    static auto constexpr LIMIT_DEFAULT = 50;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        boost::json::array nfts;
        uint32_t ledgerIndex;
        std::string issuer;
        bool validated = true;
        std::optional<uint32_t> nftTaxon;
        uint32_t limit;
        std::optional<std::string> marker;
    };

    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::string issuer;
        std::optional<uint32_t> nftTaxon;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        std::optional<std::string> marker;
        std::optional<uint32_t> limit;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new NFTsByIssuerHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    NFTsByIssuerHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    /**
     * @brief Returns the API specification for the command
     *
     * @param apiVersion The api version to return the spec for
     * @return The spec for the given apiVersion
     */
    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        static auto const rpcSpec = RpcSpec{
            {JS(issuer), validation::Required{}, validation::AccountValidator},
            {"nft_taxon", validation::Type<uint32_t>{}},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Min(1u),
             modifiers::Clamp<int32_t>{LIMIT_MIN, LIMIT_MAX}},
            {JS(marker), validation::Uint256HexStringValidator},
        };

        return rpcSpec;
    }

    /**
     * @brief Process the NFTsByIssuer command
     *
     * @param input The input data for the command
     * @param ctx The context of the request
     * @return The result of the operation
     */
    Result
    process(Input input, Context const& ctx) const;

private:
    /**
     * @brief Convert the Output to a JSON object
     *
     * @param jv The JSON object to convert to
     * @param output The output to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);

    /**
     * @brief Convert a JSON object to Input type
     *
     * @param jv The JSON object to convert
     * @return The input type
     */
    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);
};
}  // namespace rpc
