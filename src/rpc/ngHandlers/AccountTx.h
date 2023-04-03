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
class AccountTxHandler
{
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

public:
    struct Marker
    {
        uint32_t ledger;
        uint32_t seq;
    };

    struct Output
    {
        std::string account;
        uint32_t ledgerIndexMin;
        uint32_t ledgerIndexMax;
        std::optional<uint32_t> limit;
        std::optional<Marker> marker;
        // TODO: use a better type than json
        boost::json::array transactions;
        // validated should be sent via framework
        bool validated = true;
    };

    // TODO:we did not implement the "strict" field
    struct Input
    {
        std::string account;
        // You must use at least one of the following fields in your request:
        // ledger_index, ledger_hash, ledger_index_min, or ledger_index_max.
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        std::optional<int32_t> ledgerIndexMin;
        std::optional<int32_t> ledgerIndexMax;
        bool binary = false;
        bool forward = false;
        std::optional<uint32_t> limit;
        std::optional<Marker> marker;
    };

    using Result = RPCng::HandlerReturnType<Output>;

    AccountTxHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend)
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
            {JS(ledger_index_min), validation::Type<int32_t>{}},
            {JS(ledger_index_max), validation::Type<int32_t>{}},
            {JS(binary), validation::Type<bool>{}},
            {JS(forward), validation::Type<bool>{}},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Between{1, 100}},
            {JS(marker),
             validation::WithCustomError{
                 validation::Type<boost::json::object>{},
                 RPC::Status{
                     RPC::RippledError::rpcINVALID_PARAMS, "invalidMarker"}},
             validation::Section{
                 {JS(ledger),
                  validation::Required{},
                  validation::Type<uint32_t>{}},
                 {JS(seq),
                  validation::Required{},
                  validation::Type<uint32_t>{}},
             }}};
        return rpcSpec;
    }

    Result
    process(Input input, boost::asio::yield_context& yield) const;
};

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountTxHandler::Output const& output);

void
tag_invoke(
    boost::json::value_from_tag,
    boost::json::value& jv,
    AccountTxHandler::Marker const& marker);

AccountTxHandler::Input
tag_invoke(
    boost::json::value_to_tag<AccountTxHandler::Input>,
    boost::json::value const& jv);
}  // namespace RPCng
