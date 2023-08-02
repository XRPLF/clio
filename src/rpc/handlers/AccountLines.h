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
#include <rpc/common/Modifiers.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>

#include <vector>

namespace RPC {

/**
 * @brief The account_lines method returns information about an account's trust lines, which contain balances in all
 * non-XRP currencies and assets.
 *
 * For more details see: https://xrpl.org/account_lines.html
 */
class AccountLinesHandler
{
    // dependencies
    std::shared_ptr<BackendInterface> const sharedPtrBackend_;

public:
    static auto constexpr LIMIT_MIN = 10;
    static auto constexpr LIMIT_MAX = 400;
    static auto constexpr LIMIT_DEFAULT = 200;

    struct LineResponse
    {
        std::string account;
        std::string balance;
        std::string currency;
        std::string limit;
        std::string limitPeer;
        uint32_t qualityIn;
        uint32_t qualityOut;
        bool noRipple;
        bool noRipplePeer;
        std::optional<bool> authorized;
        std::optional<bool> peerAuthorized;
        std::optional<bool> freeze;
        std::optional<bool> freezePeer;
    };

    struct Output
    {
        std::string account;
        std::vector<LineResponse> lines;
        std::string ledgerHash;
        uint32_t ledgerIndex;
        bool validated = true;  // should be sent via framework
        std::optional<std::string> marker;
        uint32_t limit;
    };

    struct Input
    {
        std::string account;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        std::optional<std::string> peer;
        bool ignoreDefault = false;  // TODO: document
                                     // https://github.com/XRPLF/xrpl-dev-portal/issues/1839
        uint32_t limit = LIMIT_DEFAULT;
        std::optional<std::string> marker;
    };

    using Result = HandlerReturnType<Output>;

    AccountLinesHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion) const
    {
        static auto const rpcSpec = RpcSpec{
            {JS(account),
             validation::Required{},
             meta::WithCustomError{validation::AccountValidator, Status(RippledError::rpcACT_MALFORMED)}},
            {JS(peer), meta::WithCustomError{validation::AccountValidator, Status(RippledError::rpcACT_MALFORMED)}},
            {JS(ignore_default), validation::Type<bool>{}},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Min(1u),
             modifiers::Clamp<int32_t>{LIMIT_MIN, LIMIT_MAX}},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(marker), validation::AccountMarkerValidator},
        };

        return rpcSpec;
    }

    Result
    process(Input input, Context const& ctx) const;

private:
    void
    addLine(
        std::vector<LineResponse>& lines,
        ripple::SLE const& lineSle,
        ripple::AccountID const& account,
        std::optional<ripple::AccountID> const& peerAccount) const;

private:
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);

    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);

    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, LineResponse const& line);
};

}  // namespace RPC
