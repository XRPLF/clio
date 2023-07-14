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

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>
#include <rpc/common/MetaProcessors.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

namespace RPC {

/**
 * The gateway_balances command calculates the total balances issued by a given account, optionally excluding amounts
 * held by operational addresses.
 *
 * For more details see: https://xrpl.org/gateway_balances.html#gateway_balances
 */
class GatewayBalancesHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    struct Output
    {
        std::string ledgerHash;
        uint32_t ledgerIndex;
        std::string accountID;
        bool overflow = false;
        std::map<ripple::Currency, ripple::STAmount> sums;
        std::map<ripple::AccountID, std::vector<ripple::STAmount>> hotBalances;
        std::map<ripple::AccountID, std::vector<ripple::STAmount>> assets;
        std::map<ripple::AccountID, std::vector<ripple::STAmount>> frozenBalances;
        // validated should be sent via framework
        bool validated = true;
    };

    struct Input
    {
        std::string account;
        std::set<ripple::AccountID> hotWallets;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
    };

    using Result = HandlerReturnType<Output>;

    GatewayBalancesHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend)
        : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion) const
    {
        static auto const hotWalletValidator =
            validation::CustomValidator{[](boost::json::value const& value, std::string_view key) -> MaybeError {
                if (!value.is_string() && !value.is_array())
                    return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotStringOrArray"}};

                // wallet needs to be an valid accountID or public key
                auto const wallets = value.is_array() ? value.as_array() : boost::json::array{value};
                auto const getAccountID = [](auto const& j) -> std::optional<ripple::AccountID> {
                    if (j.is_string())
                    {
                        auto const pk = ripple::parseBase58<ripple::PublicKey>(
                            ripple::TokenType::AccountPublic, j.as_string().c_str());

                        if (pk)
                            return ripple::calcAccountID(*pk);

                        return ripple::parseBase58<ripple::AccountID>(j.as_string().c_str());
                    }

                    return {};
                };

                for (auto const& wallet : wallets)
                {
                    if (!getAccountID(wallet))
                        return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "Malformed"}};
                }

                return MaybeError{};
            }};

        static auto const rpcSpec = RpcSpec{
            {JS(account), validation::Required{}, validation::AccountValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(hotwallet), hotWalletValidator}};

        return rpcSpec;
    }

    Result
    process(Input input, Context const& ctx) const;

private:
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);

    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);
};

}  // namespace RPC
