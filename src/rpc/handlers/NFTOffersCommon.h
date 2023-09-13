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
#include <rpc/common/Modifiers.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

namespace rpc {

class NFTOffersHandlerBase
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    static auto constexpr LIMIT_MIN = 50;
    static auto constexpr LIMIT_MAX = 500;
    static auto constexpr LIMIT_DEFAULT = 250;

    struct Output
    {
        std::string nftID = {};
        std::vector<ripple::SLE> offers = {};

        // validated should be sent via framework
        bool validated = true;
        std::optional<uint32_t> limit = {};
        std::optional<std::string> marker = {};
    };

    struct Input
    {
        std::string nftID;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        uint32_t limit = LIMIT_DEFAULT;
        std::optional<std::string> marker;
    };

    using Result = HandlerReturnType<Output>;

    NFTOffersHandlerBase(std::shared_ptr<BackendInterface> const& sharedPtrBackend)
        : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion) const
    {
        static auto const rpcSpec = RpcSpec{
            {JS(nft_id), validation::Required{}, validation::Uint256HexStringValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Min(1u),
             modifiers::Clamp<int32_t>{LIMIT_MIN, LIMIT_MAX}},
            {JS(marker), validation::Uint256HexStringValidator},
        };

        return rpcSpec;
    }

protected:
    Result
    iterateOfferDirectory(
        Input input,
        ripple::uint256 const& tokenID,
        ripple::Keylet const& directory,
        boost::asio::yield_context yield) const;

private:
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);

    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);
};

}  // namespace rpc
