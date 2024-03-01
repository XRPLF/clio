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
#include "rpc/common/JsonBool.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rpc {

/**
 * @brief The account_info command retrieves information about an account, its activity, and its XRP balance.
 *
 * For more details see: https://xrpl.org/account_info.html
 */
class AccountInfoHandler {
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        uint32_t ledgerIndex;
        std::string ledgerHash;
        ripple::STLedgerEntry accountData;
        bool isDisallowIncomingEnabled = false;
        bool isClawbackEnabled = false;
        uint32_t apiVersion;
        std::optional<std::vector<ripple::STLedgerEntry>> signerLists;
        // validated should be sent via framework
        bool validated = true;

        /**
         * @brief Construct a new Output object
         *
         * @param ledgerId The ledger index
         * @param ledgerHash The ledger hash
         * @param sle The account data
         * @param isDisallowIncomingEnabled Whether disallow incoming is enabled
         * @param isClawbackEnabled Whether clawback is enabled
         * @param version The API version
         * @param signerLists The signer lists
         */
        Output(
            uint32_t ledgerId,
            std::string ledgerHash,
            ripple::STLedgerEntry sle,
            bool isDisallowIncomingEnabled,
            bool isClawbackEnabled,
            uint32_t version,
            std::optional<std::vector<ripple::STLedgerEntry>> signerLists = std::nullopt
        )
            : ledgerIndex(ledgerId)
            , ledgerHash(std::move(ledgerHash))
            , accountData(std::move(sle))
            , isDisallowIncomingEnabled(isDisallowIncomingEnabled)
            , isClawbackEnabled(isClawbackEnabled)
            , apiVersion(version)
            , signerLists(std::move(signerLists))
        {
        }
    };

    /**
     * @brief A struct to hold the input data for the command
     *
     * `queue` is not available in Reporting mode
     * `ident` is deprecated, keep it for now, in line with rippled
     */
    struct Input {
        std::optional<std::string> account;
        std::optional<std::string> ident;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        JsonBool signerLists{false};
    };

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Construct a new AccountInfoHandler object
     *
     * @param sharedPtrBackend The backend to use
     */
    AccountInfoHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
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
        static auto const rpcSpecV1 = RpcSpec{
            {JS(account), validation::AccountValidator},
            {JS(ident), validation::AccountValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator}
        };

        static auto const rpcSpec = RpcSpec{rpcSpecV1, {{JS(signer_lists), validation::Type<bool>{}}}};

        return apiVersion == 1 ? rpcSpecV1 : rpcSpec;
    }

    /**
     * @brief Process the AccountInfo command
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
