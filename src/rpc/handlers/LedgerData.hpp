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
#include "rpc/common/MetaProcessors.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"
#include "util/LedgerUtils.hpp"
#include "util/log/Logger.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

namespace rpc {

/**
 * @brief The ledger_data method retrieves contents of the specified ledger. You can iterate through several calls to
 * retrieve the entire contents of a single ledger version.
 *
 * For more details see: https://xrpl.org/ledger_data.html
 */
class LedgerDataHandler {
    // dependencies
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    util::Logger log_{"RPC"};

public:
    // constants
    static uint32_t constexpr LIMITBINARY = 2048;
    static uint32_t constexpr LIMITJSON = 256;

    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        uint32_t ledgerIndex{};
        std::string ledgerHash;
        std::optional<boost::json::object> header;
        boost::json::array states;
        std::optional<std::string> marker;
        std::optional<uint32_t> diffMarker;
        std::optional<bool> cacheFull;
        bool validated = true;
    };

    /**
     * @brief A struct to hold the input data for the command
     *
     * @note TODO: Clio does not implement `type` filter
     * @note `outOfOrder` is only for Clio, there is no document, traverse via seq diff (outOfOrder implementation is
     * copied from old rpc handler)
     */
    struct Input {
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        bool binary = false;
        uint32_t limit = LedgerDataHandler::LIMITJSON;  // max 256 for json ; 2048 for binary
        std::optional<ripple::uint256> marker;
        std::optional<uint32_t> diffMarker;
        bool outOfOrder = false;
        ripple::LedgerEntryType type = ripple::LedgerEntryType::ltANY;
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new LedgerDataHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    LedgerDataHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
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
        auto const& ledgerTypeStrs = util::getLedgerEntryTypeStrs();
        static auto const rpcSpec = RpcSpec{
            {JS(binary), validation::Type<bool>{}},
            {"out_of_order", validation::Type<bool>{}},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(limit), validation::Type<uint32_t>{}, validation::Min(1u)},
            {JS(marker),
             validation::Type<uint32_t, std::string>{},
             meta::IfType<std::string>{validation::Uint256HexStringValidator}},
            {JS(type),
             meta::WithCustomError{
                 validation::Type<std::string>{}, Status{ripple::rpcINVALID_PARAMS, "Invalid field 'type', not string."}
             },
             validation::OneOf<std::string>(ledgerTypeStrs.cbegin(), ledgerTypeStrs.cend())},

        };
        return rpcSpec;
    }

    /**
     * @brief Process the LedgerData command
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
     * @return Input parsed from the JSON object
     */
    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);
};
}  // namespace rpc
