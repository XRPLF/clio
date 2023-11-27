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

#include "data/BackendInterface.h"
#include "rpc/RPCHelpers.h"
#include "rpc/common/JsonBool.h"
#include "rpc/common/MetaProcessors.h"
#include "rpc/common/Modifiers.h"
#include "rpc/common/Types.h"
#include "rpc/common/Validators.h"
#include "util/log/Logger.h"

namespace rpc {

/**
 * @brief The account_tx method retrieves a list of transactions that involved the specified account.
 *
 * For more details see: https://xrpl.org/account_tx.html
 */
class AccountTxHandler {
    util::Logger log_{"RPC"};
    std::shared_ptr<BackendInterface> sharedPtrBackend_;

    static std::unordered_map<std::string, ripple::TxType> const TYPESMAP;
    static std::unordered_set<std::string> const TYPES_KEYS;

public:
    // no max limit
    static auto constexpr LIMIT_MIN = 1;
    static auto constexpr LIMIT_DEFAULT = 200;

    struct Marker {
        uint32_t ledger;
        uint32_t seq;
    };

    struct Output {
        std::string account;
        uint32_t ledgerIndexMin{0};
        uint32_t ledgerIndexMax{0};
        std::optional<uint32_t> limit;
        std::optional<Marker> marker;
        // TODO: use a better type than json
        boost::json::array transactions;
        // validated should be sent via framework
        bool validated = true;
    };

    struct Input {
        std::string account;
        // You must use at least one of the following fields in your request:
        // ledger_index, ledger_hash, ledger_index_min, or ledger_index_max.
        std::optional<std::string> ledgerHash;
        std::optional<uint32_t> ledgerIndex;
        std::optional<int32_t> ledgerIndexMin;
        std::optional<int32_t> ledgerIndexMax;
        bool usingValidatedLedger = false;
        JsonBool binary{false};
        JsonBool forward{false};
        std::optional<uint32_t> limit;
        std::optional<Marker> marker;
        std::optional<ripple::TxType> transactionType;
    };

    using Result = HandlerReturnType<Output>;

    AccountTxHandler(std::shared_ptr<BackendInterface> const& sharedPtrBackend) : sharedPtrBackend_(sharedPtrBackend)
    {
    }

    static RpcSpecConstRef
    spec([[maybe_unused]] uint32_t apiVersion)
    {
        static auto const rpcSpecForV1 = RpcSpec{
            {JS(account), validation::Required{}, validation::AccountValidator},
            {JS(ledger_hash), validation::Uint256HexStringValidator},
            {JS(ledger_index), validation::LedgerIndexValidator},
            {JS(ledger_index_min), validation::Type<int32_t>{}},
            {JS(ledger_index_max), validation::Type<int32_t>{}},
            {JS(limit),
             validation::Type<uint32_t>{},
             validation::Min(1u),
             modifiers::Clamp<int32_t>{LIMIT_MIN, std::numeric_limits<int32_t>::max()}},
            {JS(marker),
             meta::WithCustomError{
                 validation::Type<boost::json::object>{},
                 Status{RippledError::rpcINVALID_PARAMS, "invalidMarker"},
             },
             meta::Section{
                 {JS(ledger), validation::Required{}, validation::Type<uint32_t>{}},
                 {JS(seq), validation::Required{}, validation::Type<uint32_t>{}},
             }},
            {
                "tx_type",
                validation::Type<std::string>{},
                modifiers::ToLower{},
                validation::OneOf<std::string>(TYPES_KEYS.cbegin(), TYPES_KEYS.cend()),
            },
        };

        static auto const rpcSpec = RpcSpec{
            rpcSpecForV1,
            {
                {JS(binary), validation::Type<bool>{}},
                {JS(forward), validation::Type<bool>{}},
            }
        };

        return apiVersion == 1 ? rpcSpecForV1 : rpcSpec;
    }

    Result
    process(Input input, Context const& ctx) const;

private:
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output);

    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv);

    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Marker const& marker);
};
}  // namespace rpc
