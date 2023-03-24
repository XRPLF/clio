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

#include <boost/asio/spawn.hpp>

namespace RPCng {
class NFTInfoHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    struct Output
    {
        std::string nftID;
        uint32_t ledgerIndex;
        std::string owner;
        bool isBurned;
        uint32_t flags;
        uint32_t transferFee;
        std::string issuer;
        uint32_t taxon;
        uint32_t
            serial;  // TODO: documented as 'nft_sequence' atm.
                     // https://github.com/XRPLF/xrpl-dev-portal/issues/1841
        std::optional<std::string> uri;

        // validated should be sent via framework
        bool validated = true;
    };

    struct Input
    {
        std::string nftID;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
    };

    using Result = RPCng::HandlerReturnType<Output>;

    NFTInfoHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend)
        : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec() const
    {
        static auto const rpcSpec = RpcSpec{
            {JS(nft_id),
             validation::Required{},
             validation::Uint256HexStringValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
        };
        return rpcSpec;
    }

    Result
    process(Input input, boost::asio::yield_context& yield) const;
};

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    NFTInfoHandler::Output const& output);

NFTInfoHandler::Input
tag_invoke(
    boost::json::value_to_tag<NFTInfoHandler::Input>,
    boost::json::value const& jv);
}  // namespace RPCng
