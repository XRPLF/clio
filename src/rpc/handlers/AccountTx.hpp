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
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/JsonBool.hpp"
#include "rpc/common/MetaProcessors.hpp"
#include "rpc/common/Modifiers.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"
#include "util/TxUtils.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/TxFormats.h>
#include <ripple/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

namespace rpc {

/**
 * @brief The account_tx method retrieves a list of transactions that involved the specified account.
 *
 * For more details see: https://xrpl.org/account_tx.html
 */
class AccountTxHandler {
    util::Logger log_{"RPC"};
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    static auto constexpr LIMIT_MIN = 1;
    static auto constexpr LIMIT_MAX = 1000;
    static auto constexpr LIMIT_DEFAULT = 200;

    /**
     * @brief A struct to hold the marker data
     */
    struct Marker {
        uint32_t ledger;
        uint32_t seq;
    };

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        std::string account;
        uint32_t ledgerIndexMin{0};
        uint32_t ledgerIndexMax{0};
        std::optional<uint32_t> limit;
        std::optional<Marker> marker;
        // TODO: use a better type than json
        boost::json::array transactions;
        // validated should be sent via framework
        bool validated = true;
    };

    /**
     * @brief A struct to hold the input data for the command
     */
    struct Input {
        std::string account;
        // You must use at least one of the following fields in your request:
        // ledger_index, ledger_hash, ledger_index_min, or ledger_index_max.
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        std::optional<int32_t> ledgerIndexMin;
        std::optional<int32_t> ledgerIndexMax;
        bool usingValidatedLedger = false;
        JsonBool binary{false};
        JsonBool forward{false};
        std::optional<uint32_t> limit;
        std::optional<Marker> marker;
        std::optional<std::string> transactionTypeInLowercase;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new AccountTxHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    AccountTxHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
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
        auto const& typesKeysInLowercase = util::getTxTypesInLowercase();
        static auto const rpcSpecForV1 = RpcSpec{
            {JS(account), validation::Required{}, validation::AccountValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(ledger_index_min), validation::Type<int32_t>{}},
            {JS(ledger_index_max), validation::Type<int32_t>{}},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Min(1u),
             modifiers::Clamp<int32_t>{LIMIT_MIN, LIMIT_MAX}},
            {JS(marker),
             meta::WithCustomError{
                 validation::Type<boost::json::object>{},
                 Status{RippledError::rpcINVALID_PARAMS, "invalidMarker"},
             },
             meta::Section{
                 {JS(ledger), validation::Required{}, validation::Type<uint32_t>{}},
                 {JS(seq), validation::Required{}, validation::Type<uint32_t>{}},
             }},
            {
                "tx_type",
                validation::Type<std::string>{},
                modifiers::ToLower{},
                validation::OneOf<std::string>(typesKeysInLowercase.cbegin(), typesKeysInLowercase.cend()),
            },
        };

        static auto const rpcSpec = RpcSpec{
            rpcSpecForV1,
            {
                {JS(binary), validation::Type<bool>{}},
                {JS(forward), validation::Type<bool>{}},
            }
        };

        return apiVersion == 1 ? rpcSpecForV1 : rpcSpec;
    }

    /**
     * @brief Process the AccountTx command
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

    /**
     * @brief Convert the Marker to a JSON object
     *
     * @param [out] jv The JSON object to convert to
     * @param marker The marker to convert
     */
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Marker const& marker);
};
}  // namespace rpc
