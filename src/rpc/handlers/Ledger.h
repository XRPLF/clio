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
 * @brief Retrieve information about the public ledger.
 *
 * For more details see: https://xrpl.org/ledger.html
 */
class LedgerHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    struct Output
    {
        uint32_t ledgerIndex;
        std::string ledgerHash;
        // TODO: use better type
        boost::json::object header;
        bool validated = true;
    };

    // clio not support : accounts/full/owner_finds/queue/type
    // clio will throw error when accounts/full/owner_funds/queue is set to true
    // https://github.com/XRPLF/clio/issues/603
    struct Input
    {
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        bool binary = false;
        bool expand = false;
        bool ownerFunds = false;
        bool transactions = false;
        bool diff = false;
    };

    using Result = HandlerReturnType<Output>;

    LedgerHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion) const
    {
        static auto const rpcSpec = RpcSpec{
            {JS(full), validation::Type<bool>{}, validation::NotSupported{true}},
            {JS(accounts), validation::Type<bool>{}, validation::NotSupported{true}},
            {JS(owner_funds), validation::Type<bool>{}},
            {JS(queue), validation::Type<bool>{}, validation::NotSupported{true}},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(transactions), validation::Type<bool>{}},
            {JS(expand), validation::Type<bool>{}},
            {JS(binary), validation::Type<bool>{}},
            {"diff", validation::Type<bool>{}},
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
