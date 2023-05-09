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
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

namespace RPC {
/**
 * @brief The account_info command retrieves information about an account, its activity, and its XRP balance.
 * https://xrpl.org/account_info.html
 */
class AccountInfoHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    struct Output
    {
        uint32_t ledgerIndex;
        std::string ledgerHash;
        ripple::STLedgerEntry accountData;
        std::optional<std::vector<ripple::STLedgerEntry>> signerLists;
        // validated should be sent via framework
        bool validated = true;

        Output(
            uint32_t ledgerId,
            std::string ledgerHash,
            ripple::STLedgerEntry sle,
            std::vector<ripple::STLedgerEntry> signerLists)
            : ledgerIndex(ledgerId)
            , ledgerHash(std::move(ledgerHash))
            , accountData(std::move(sle))
            , signerLists(std::move(signerLists))
        {
        }

        Output(uint32_t ledgerId, std::string ledgerHash, ripple::STLedgerEntry sle)
            : ledgerIndex(ledgerId), ledgerHash(std::move(ledgerHash)), accountData(std::move(sle))
        {
        }
    };

    // "queue" is not available in Reporting mode
    // "ident" is deprecated, keep it for now, in line with rippled
    struct Input
    {
        std::optional<std::string> account;
        std::optional<std::string> ident;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        bool signerLists = false;
    };

    using Result = HandlerReturnType<Output>;

    AccountInfoHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec() const
    {
        static auto const rpcSpec = RpcSpec{
            {JS(account), validation::AccountValidator},
            {JS(ident), validation::AccountValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(signer_lists), validation::Type<bool>{}},
        };

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
