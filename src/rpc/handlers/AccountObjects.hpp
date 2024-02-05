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

#include "data/BackendInterface.hpp"
#include "rpc/JS.hpp"
#include "rpc/common/Modifiers.hpp"
#include "rpc/common/Specs.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/Validators.hpp"
#include "util/LedgerUtils.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <ripple/protocol/LedgerFormats.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/jss.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rpc {

/**
 * @brief The account_objects command returns the raw ledger format for all objects owned by an account.
 * The results can be filtered by the type.
 * The valid types are: check, deposit_preauth, escrow, nft_offer, offer, payment_channel, signer_list, state (trust
 * line), did and ticket.
 *
 * For more details see: https://xrpl.org/account_objects.html
 */
class AccountObjectsHandler {
    // dependencies
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

    // constants
    static std::unordered_map<std::string, ripple::LedgerEntryType> const TYPES_MAP;
    static std::unordered_set<std::string> const TYPES_KEYS;

public:
    static auto constexpr LIMIT_MIN = 10;
    static auto constexpr LIMIT_MAX = 400;
    static auto constexpr LIMIT_DEFAULT = 200;

    struct Output {
        std::string account;
        std::string ledgerHash;
        uint32_t ledgerIndex{};
        std::optional<std::string> marker;
        uint32_t limit{};
        std::vector<ripple::SLE> accountObjects;
        bool validated = true;
    };

    struct Input {
        std::string account;
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        uint32_t limit = LIMIT_DEFAULT;  // [10,400]
        std::optional<std::string> marker;
        std::optional<ripple::LedgerEntryType> type;
        bool deletionBlockersOnly = false;
    };

    using Result = HandlerReturnType<Output>;

    AccountObjectsHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend)
        : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        auto const& ledgerTypeStrs = util::getLedgerEntryTypeStrs();
        static auto const rpcSpec = RpcSpec{
            {JS(account), validation::Required{}, validation::AccountValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Min(1u),
             modifiers::Clamp<int32_t>(LIMIT_MIN, LIMIT_MAX)},
            {JS(type),
             validation::Type<std::string>{},
             validation::OneOf<std::string>(ledgerTypeStrs.cbegin(), ledgerTypeStrs.cend())},
            {JS(marker), validation::AccountMarkerValidator},
            {JS(deletion_blockers_only), validation::Type<bool>{}},
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
