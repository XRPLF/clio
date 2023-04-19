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

#include <set>

namespace RPCng {
class AccountObjectsHandler
{
    // dependencies
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    struct Output
    {
        std::string account;
        std::string ledgerHash;
        uint32_t ledgerIndex;
        std::optional<std::string> marker;
        uint32_t limit;
        std::vector<ripple::SLE> accountObjects;
        bool validated = true;
    };

    // Clio does not implement deletion_blockers_only
    struct Input
    {
        std::string account;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        uint32_t limit = 200;  //[10,400]
        std::optional<std::string> marker;
        std::optional<ripple::LedgerEntryType> type;
    };

    using Result = RPCng::HandlerReturnType<Output>;

    AccountObjectsHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend)
        : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec() const
    {
        static auto const rpcSpec = RpcSpec{
            {JS(account), validation::Required{}, validation::AccountValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(limit), validation::Type<uint32_t>{}, validation::Between(10, 400)},
            {JS(type),
             validation::Type<std::string>{},
             validation::OneOf<std::string>{
                 "state",
                 "ticket",
                 "signer_list",
                 "payment_channel",
                 "offer",
                 "escrow",
                 "deposit_preauth",
                 "check",
                 "nft_page",
                 "nft_offer"}},
            {JS(marker), validation::AccountMarkerValidator},
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

    static std::unordered_map<std::string, ripple::LedgerEntryType> const TYPESMAP;
};
}  // namespace RPCng
