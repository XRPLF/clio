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
#include <rpc/JS.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

#include <vector>

namespace RPC {

/**
 * @brief The deposit_authorized command indicates whether one account is authorized to send payments directly to
 * another. See Deposit Authorization for information on how to require authorization to deliver money to your account.
 *
 * For more details see: https://xrpl.org/deposit_authorized.html
 */
class DepositAuthorizedHandler
{
    // dependencies
    std::shared_ptr<BackendInterface> const sharedPtrBackend_;

public:
    // Note: `ledger_current_index` is omitted because it only makes sense for rippled
    struct Output
    {
        bool depositAuthorized = true;
        std::string sourceAccount;
        std::string destinationAccount;
        std::string ledgerHash;
        uint32_t ledgerIndex;
        // validated should be sent via framework
        bool validated = true;
    };

    struct Input
    {
        std::string sourceAccount;
        std::string destinationAccount;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
    };

    using Result = HandlerReturnType<Output>;

    DepositAuthorizedHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend)
        : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion) const
    {
        static auto const rpcSpec = RpcSpec{
            {JS(source_account), validation::Required{}, validation::AccountValidator},
            {JS(destination_account), validation::Required{}, validation::AccountValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
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
