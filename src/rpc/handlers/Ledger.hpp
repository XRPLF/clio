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
#include "rpc/common/Checkers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <xrpl/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace rpc {

/**
 * @brief Retrieve information about the public ledger.
 *
 * For more details see: https://xrpl.org/ledger.html
 */
class LedgerHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        uint32_t ledgerIndex{};
        std::string ledgerHash;
        // TODO: use better type
        boost::json::object header;
        bool validated = true;
    };

    /**
     * @brief A struct to hold the input data for the command
     *
     * Clio does not support:
     * - queue
     *
     * And the following are deprecated altogether:
     * - full
     * - accounts
     * - ledger
     * - type
     *
     * Clio will throw an error when `queue` is set to `true`
     * or if `full` or `accounts` are used.
     * @see https://github.com/XRPLF/clio/issues/603
     */
    struct Input {
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        bool binary = false;
        bool expand = false;
        bool ownerFunds = false;
        bool transactions = false;
        bool diff = false;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new LedgerHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    LedgerHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
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
            {JS(full), validation::NotSupported{}},
            {JS(full), check::Deprecated{}},
            {JS(accounts), validation::NotSupported{}},
            {JS(accounts), check::Deprecated{}},
            {JS(owner_funds), validation::Type<bool>{}},
            {JS(queue), validation::Type<bool>{}, validation::NotSupported{true}},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(transactions), validation::Type<bool>{}},
            {JS(expand), validation::Type<bool>{}},
            {JS(binary), validation::Type<bool>{}},
            {"diff", validation::Type<bool>{}},
            {JS(ledger), check::Deprecated{}},
            {JS(type), check::Deprecated{}},
        };

        return rpcSpec;
    }

    /**
     * @brief Process the Ledger command
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
