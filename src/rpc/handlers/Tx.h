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

#include <data/BackendInterface.h>
#include <rpc/RPCHelpers.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

namespace rpc {

/**
 * @brief The tx method retrieves information on a single transaction, by its identifying hash.
 *
 * For more details see: https://xrpl.org/tx.html
 */
class TxHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    struct Output
    {
        uint32_t date = 0u;
        std::string hash{};
        uint32_t ledgerIndex = 0u;
        std::optional<boost::json::object> meta{};
        std::optional<boost::json::object> tx{};
        std::optional<std::string> metaStr{};
        std::optional<std::string> txStr{};
        uint32_t apiVersion = 0u;
        bool validated = true;
    };

    struct Input
    {
        std::string transaction;
        bool binary = false;
        std::optional<uint32_t> minLedger;
        std::optional<uint32_t> maxLedger;
    };

    using Result = HandlerReturnType<Output>;

    TxHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion) const
    {
        static const RpcSpec rpcSpec = {
            {JS(transaction), validation::Required{}, validation::Uint256HexStringValidator},
            {JS(binary), validation::Type<bool>{}},
            {JS(min_ledger), validation::Type<uint32_t>{}},
            {JS(max_ledger), validation::Type<uint32_t>{}},
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
}  // namespace rpc
