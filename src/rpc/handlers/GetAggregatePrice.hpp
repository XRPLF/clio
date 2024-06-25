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
#include "rpc/common/Modifiers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <xrpl/basics/Number.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rpc {

/**
 *@brief The get_aggregate_price method.
 */
class GetAggregatePriceHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief A struct to hold the statistics
     */
    struct Stats {
        ripple::STAmount avg;
        ripple::Number sd;  // standard deviation
        uint32_t size{0};
    };

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        uint32_t time;
        Stats extireStats;
        std::optional<Stats> trimStats;
        std::string ledgerHash;
        uint32_t ledgerIndex;
        std::string median;
        bool validated = true;

        /**
         * @brief Construct a new Output object
         * @param time The time of the latest oracle data
         * @param ledgerHash The hash of the ledger
         * @param ledgerIndex The index of the ledger
         */
        Output(uint32_t time, std::string ledgerHash, uint32_t ledgerIndex)
            : time(time), ledgerHash(std::move(ledgerHash)), ledgerIndex(ledgerIndex)
        {
        }
    };

    /**
     * @brief A struct to hold the input oracle data
     */
    struct Oracle {
        std::uint32_t documentId{0};
        ripple::AccountID account;
    };

    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::optional<std::string> ledgerHash;
        std::optional<std::uint32_t> ledgerIndex;
        std::vector<Oracle> oracles;  // valid range is 1-200
        std::string baseAsset;
        std::string quoteAsset;
        std::optional<std::uint32_t> timeThreshold;
        std::optional<std::uint8_t> trim;  // valid range is 1-25
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
        static auto constexpr ORACLES_MAX = 200;

        static auto const oraclesValidator =
            modifiers::CustomModifier{[](boost::json::value& value, std::string_view) -> MaybeError {
                if (!value.is_array() or value.as_array().empty() or value.as_array().size() > ORACLES_MAX)
                    return Error{Status{RippledError::rpcORACLE_MALFORMED}};

                for (auto& oracle : value.as_array()) {
                    if (!oracle.is_object() or !oracle.as_object().contains(JS(oracle_document_id)) or
                        !oracle.as_object().contains(JS(account)))
                        return Error{Status{RippledError::rpcORACLE_MALFORMED}};

                    auto maybeError = validation::Type<std::uint32_t, std::string>{}.verify(
                        oracle.as_object(), JS(oracle_document_id)
                    );
                    if (!maybeError)
                        return maybeError;

                    maybeError = modifiers::ToNumber::modify(oracle, JS(oracle_document_id));
                    if (!maybeError)
                        return maybeError;

                    maybeError = validation::AccountBase58Validator.verify(oracle.as_object(), JS(account));
                    if (!maybeError)
                        return Error{Status{RippledError::rpcINVALID_PARAMS}};
                };

                return MaybeError{};
            }};

        static auto const rpcSpec = RpcSpec{
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            // validate quoteAsset and base_asset in accordance to the currency code found in XRPL doc:
            // https://xrpl.org/docs/references/protocol/data-types/currency-formats#currency-codes
            // usually Clio returns rpcMALFORMED_CURRENCY , return InvalidParam here just to mimic rippled
            {JS(base_asset),
             validation::Required{},
             meta::WithCustomError{validation::CurrencyValidator, Status(RippledError::rpcINVALID_PARAMS)}},
            {JS(quote_asset),
             validation::Required{},
             meta::WithCustomError{validation::CurrencyValidator, Status(RippledError::rpcINVALID_PARAMS)}},
            {JS(oracles), validation::Required{}, oraclesValidator},
            // note: Unlike `rippled`, Clio only supports UInt as input, no string, no `null`, etc.
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
     * @brief Calls callback on the oracle ledger entry
     If the oracle entry does not contains the price pair, search up to three previous metadata objects. Stops early if
     the callback returns true.
     */
    void
    tracebackOracleObject(
        boost::asio::yield_context yield,
        ripple::STObject const& oracleObject,
        std::function<bool(ripple::STObject const&)> const& callback
    ) const;

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
