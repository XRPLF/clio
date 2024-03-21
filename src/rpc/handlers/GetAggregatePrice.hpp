//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/MetaProcessors.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
namespace rpc {

/**
 *@brief The get_aggregate_price method.
 */
class GetAggregatePriceHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string ledgerHash;
        uint32_t ledgerIndex;
        bool validated = true;
    };

    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::optional<std::string> ledgerHash;
        std::optional<std::uint32_t> ledgerIndex;
        boost::json::array oracles;  // valid size is 1-200
        std::string baseAsset;
        std::string quoteAsset;
        std::uint32_t timeThreshold;
        std::uint8_t trim;  // valid 1-25
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new GetAggregatePrice handler object
     *
     * @param sharedPtrBackend The backend to use
     */
    GetAggregatePriceHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend)
        : sharedPtrBackend_(sharedPtrBackend)
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
        static auto constexpr maxOracles = 200;

        static auto const oraclesValidator =
            validation::CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
                if (!value.is_array() or value.as_array().empty() or value.as_array().size() > maxOracles)
                    return Error{Status{RippledError::rpcORACLE_MALFORMED}};

                auto const oracleJsonValidator = meta::Section{
                    {JS(oracle_document_id), validation::Required{}, validation::Type<std::string>{}},
                    {JS(account), validation::Required{}, validation::AccountBase58Validator},
                };

                for (auto oracle : value.as_array()) {
                    if (!oracle.is_object() or !oracle.as_object().contains(JS(oracle_document_id)) or
                        !oracle.as_object().contains(JS(account)))
                        return Error{Status{RippledError::rpcORACLE_MALFORMED}};

                    auto maybeError = oracleJsonValidator.verify(oracle, key);
                    if (!maybeError)
                        return maybeError;
                }

                return MaybeError{};
            }};

        static auto const rpcSpec = RpcSpec{
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(base_asset), validation::Required{}, validation::Type<std::string>{}},
            {JS(quote_asset), validation::Required{}, validation::Type<std::string>{}},
            {JS(oracles), validation::Required{}, oraclesValidator},
            {JS(time_threshold), validation::Type<std::uint32_t>{}},
            {
                JS(trim),
                validation::Type<std::uint8_t>{},
                validation::Between<std::uint8_t>{1, 25},
            }
        };

        return rpcSpec;
    }

    /**
     * @brief Process the GetAggregatePrice command
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
     * @param [out] jv The JSON object to convert to
     * @param output The output to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);

    /**
     * @brief Convert a JSON object to Input type
     *
     * @param jv The JSON object to convert
     * @return Input parsed from the JSON object
     */
    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);
};

}  // namespace rpc
