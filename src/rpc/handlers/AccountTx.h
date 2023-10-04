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
#include <etl/ETLService.h>
#include <rpc/RPCHelpers.h>
#include <rpc/common/JsonBool.h>
#include <rpc/common/MetaProcessors.h>
#include <rpc/common/Modifiers.h>
#include <rpc/common/Types.h>
#include <rpc/common/Validators.h>
#include <util/Profiler.h>
#include <util/log/Logger.h>

namespace rpc {

template <typename ETLServiceType>
class BaseAccountTxHandler
{
    util::Logger log_{"RPC"};
    std::shared_ptr<BackendInterface> sharedPtrBackend_;
    std::shared_ptr<ETLServiceType const> etl_;

    static std::unordered_map<std::string, ripple::TxType> const TYPESMAP;
    static const std::unordered_set<std::string> TYPES_KEYS;

public:
    // no max limit
    static auto constexpr LIMIT_MIN = 1;
    static auto constexpr LIMIT_DEFAULT = 200;

    struct Marker
    {
        uint32_t ledger;
        uint32_t seq;
    };

    struct Output
    {
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

    struct Input
    {
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

    BaseAccountTxHandler(
        std::shared_ptr<BackendInterface> const& sharedPtrBackend,
        std::shared_ptr<ETLServiceType const> const& etl)
        : sharedPtrBackend_(sharedPtrBackend), etl_(etl)
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
            }};

        return apiVersion == 1 ? rpcSpecForV1 : rpcSpec;
    }

    // TODO: this is currently very similar to nft_history but its own copy for time
    // being. we should aim to reuse common logic in some way in the future.
    Result
    process(BaseAccountTxHandler::Input input, Context const& ctx) const
    {
        auto const range = sharedPtrBackend_->fetchLedgerRange();
        auto [minIndex, maxIndex] = *range;

        if (input.ledgerIndexMin)
        {
            if (ctx.apiVersion > 1u &&
                (input.ledgerIndexMin > range->maxSequence || input.ledgerIndexMin < range->minSequence))
            {
                return Error{Status{RippledError::rpcLGR_IDX_MALFORMED, "ledgerSeqMinOutOfRange"}};
            }

            if (static_cast<std::uint32_t>(*input.ledgerIndexMin) > minIndex)
                minIndex = *input.ledgerIndexMin;
        }

        if (input.ledgerIndexMax)
        {
            if (ctx.apiVersion > 1u &&
                (input.ledgerIndexMax > range->maxSequence || input.ledgerIndexMax < range->minSequence))
            {
                return Error{Status{RippledError::rpcLGR_IDX_MALFORMED, "ledgerSeqMaxOutOfRange"}};
            }

            if (static_cast<std::uint32_t>(*input.ledgerIndexMax) < maxIndex)
                maxIndex = *input.ledgerIndexMax;
        }

        if (minIndex > maxIndex)
        {
            if (ctx.apiVersion == 1u)
                return Error{Status{RippledError::rpcLGR_IDXS_INVALID}};

            return Error{Status{RippledError::rpcINVALID_LGR_RANGE}};
        }

        if (input.ledgerHash || input.ledgerIndex || input.usingValidatedLedger)
        {
            if (ctx.apiVersion > 1u && (input.ledgerIndexMax || input.ledgerIndexMin))
                return Error{Status{RippledError::rpcINVALID_PARAMS, "containsLedgerSpecifierAndRange"}};

            auto const lgrInfoOrStatus = getLedgerInfoFromHashOrSeq(
                *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence);

            if (auto status = std::get_if<Status>(&lgrInfoOrStatus))
                return Error{*status};

            maxIndex = minIndex = std::get<ripple::LedgerHeader>(lgrInfoOrStatus).seq;
        }

        std::optional<data::TransactionsCursor> cursor;

        // if marker exists
        if (input.marker)
        {
            cursor = {input.marker->ledger, input.marker->seq};
        }
        else
        {
            // if forward, start at minIndex - 1, because the SQL query is exclusive, we need to include the 0
            // transaction index of minIndex
            if (input.forward)
            {
                cursor = {minIndex - 1, std::numeric_limits<int32_t>::max()};
            }
            else
            {
                cursor = {maxIndex, std::numeric_limits<int32_t>::max()};
            }
        }

        auto const limit = input.limit.value_or(LIMIT_DEFAULT);
        auto const accountID = accountFromStringStrict(input.account);
        auto const [txnsAndCursor, timeDiff] = util::timed([&]() {
            return sharedPtrBackend_->fetchAccountTransactions(*accountID, limit, input.forward, cursor, ctx.yield);
        });

        LOG(log_.info()) << "db fetch took " << timeDiff << " milliseconds - num blobs = " << txnsAndCursor.txns.size();

        auto const [blobs, retCursor] = txnsAndCursor;
        Output response;

        if (retCursor)
            response.marker = {retCursor->ledgerSequence, retCursor->transactionIndex};

        for (auto const& txnPlusMeta : blobs)
        {
            // over the range
            if ((txnPlusMeta.ledgerSequence < minIndex && !input.forward) ||
                (txnPlusMeta.ledgerSequence > maxIndex && input.forward))
            {
                response.marker = std::nullopt;
                break;
            }
            if (txnPlusMeta.ledgerSequence > maxIndex && !input.forward)
            {
                LOG(log_.debug()) << "Skipping over transactions from incomplete ledger";
                continue;
            }

            boost::json::object obj;
            if (!input.binary)
            {
                auto [txn, meta] = toExpandedJson(txnPlusMeta, NFTokenjson::ENABLE, etl_->getNetworkID());
                obj[JS(meta)] = std::move(meta);
                obj[JS(tx)] = std::move(txn);

                if (obj[JS(tx)].as_object().contains(JS(TransactionType)))
                {
                    auto const objTransactionType = obj[JS(tx)].as_object()[JS(TransactionType)];
                    auto const strType = util::toLower(objTransactionType.as_string().c_str());

                    // if transactionType does not match
                    if (input.transactionType.has_value() && BaseAccountTxHandler::TYPESMAP.contains(strType) &&
                        BaseAccountTxHandler::TYPESMAP.at(strType) != input.transactionType.value())
                        continue;
                }

                obj[JS(tx)].as_object()[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
                obj[JS(tx)].as_object()[JS(date)] = txnPlusMeta.date;
            }
            else
            {
                obj[JS(meta)] = ripple::strHex(txnPlusMeta.metadata);
                obj[JS(tx_blob)] = ripple::strHex(txnPlusMeta.transaction);
                obj[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
            }

            obj[JS(validated)] = true;

            response.transactions.push_back(obj);
        }

        response.limit = input.limit;
        response.account = ripple::to_string(*accountID);
        response.ledgerIndexMin = minIndex;
        response.ledgerIndexMax = maxIndex;

        return response;
    }

private:
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output)
    {
        jv = {
            {JS(account), output.account},
            {JS(ledger_index_min), output.ledgerIndexMin},
            {JS(ledger_index_max), output.ledgerIndexMax},
            {JS(transactions), output.transactions},
            {JS(validated), output.validated},
        };

        if (output.marker)
            jv.as_object()[JS(marker)] = boost::json::value_from(*(output.marker));

        if (output.limit)
            jv.as_object()[JS(limit)] = *(output.limit);
    }

    friend Input
    tag_invoke(boost::json::value_to_tag<Input>, boost::json::value const& jv)
    {
        auto input = BaseAccountTxHandler::Input{};
        auto const& jsonObject = jv.as_object();

        input.account = jsonObject.at(JS(account)).as_string().c_str();

        if (jsonObject.contains(JS(ledger_index_min)) && jsonObject.at(JS(ledger_index_min)).as_int64() != -1)
            input.ledgerIndexMin = jsonObject.at(JS(ledger_index_min)).as_int64();

        if (jsonObject.contains(JS(ledger_index_max)) && jsonObject.at(JS(ledger_index_max)).as_int64() != -1)
            input.ledgerIndexMax = jsonObject.at(JS(ledger_index_max)).as_int64();

        if (jsonObject.contains(JS(ledger_hash)))
            input.ledgerHash = jsonObject.at(JS(ledger_hash)).as_string().c_str();

        if (jsonObject.contains(JS(ledger_index)))
        {
            if (!jsonObject.at(JS(ledger_index)).is_string())
            {
                input.ledgerIndex = jsonObject.at(JS(ledger_index)).as_int64();
            }
            else if (jsonObject.at(JS(ledger_index)).as_string() != "validated")
            {
                input.ledgerIndex = std::stoi(jsonObject.at(JS(ledger_index)).as_string().c_str());
            }
            else
            {
                // could not get the latest validated ledger seq here, using this flag to indicate that
                input.usingValidatedLedger = true;
            }
        }

        if (jsonObject.contains(JS(binary)))
            input.binary = boost::json::value_to<JsonBool>(jsonObject.at(JS(binary)));

        if (jsonObject.contains(JS(forward)))
            input.forward = boost::json::value_to<JsonBool>(jsonObject.at(JS(forward)));

        if (jsonObject.contains(JS(limit)))
            input.limit = jsonObject.at(JS(limit)).as_int64();

        if (jsonObject.contains(JS(marker)))
        {
            input.marker = BaseAccountTxHandler::Marker{
                jsonObject.at(JS(marker)).as_object().at(JS(ledger)).as_int64(),
                jsonObject.at(JS(marker)).as_object().at(JS(seq)).as_int64()};
        }

        if (jsonObject.contains("tx_type"))
        {
            auto objTransactionType = jsonObject.at("tx_type");
            input.transactionType = BaseAccountTxHandler::TYPESMAP.at(objTransactionType.as_string().c_str());
        }

        return input;
    }

    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Marker const& marker)
    {
        jv = {
            {JS(ledger), marker.ledger},
            {JS(seq), marker.seq},
        };
    }
};

// found here : https://xrpl.org/transaction-types.html
// TODO [https://github.com/XRPLF/clio/issues/856]: add AMMBid, AMMCreate, AMMDelete, AMMDeposit, AMMVote, AMMWithdraw
template <typename ETLServiceType>
std::unordered_map<std::string, ripple::TxType> const BaseAccountTxHandler<ETLServiceType>::TYPESMAP{
    {JSL(AccountSet), ripple::ttACCOUNT_SET},
    {JSL(AccountDelete), ripple::ttACCOUNT_DELETE},
    {JSL(CheckCancel), ripple::ttCHECK_CANCEL},
    {JSL(CheckCash), ripple::ttCHECK_CASH},
    {JSL(CheckCreate), ripple::ttCHECK_CREATE},
    {JSL(Clawback), ripple::ttCLAWBACK},
    {JSL(DepositPreauth), ripple::ttDEPOSIT_PREAUTH},
    {JSL(EscrowCancel), ripple::ttESCROW_CANCEL},
    {JSL(EscrowCreate), ripple::ttESCROW_CREATE},
    {JSL(EscrowFinish), ripple::ttESCROW_FINISH},
    {JSL(NFTokenAcceptOffer), ripple::ttNFTOKEN_ACCEPT_OFFER},
    {JSL(NFTokenBurn), ripple::ttNFTOKEN_BURN},
    {JSL(NFTokenCancelOffer), ripple::ttNFTOKEN_CANCEL_OFFER},
    {JSL(NFTokenCreateOffer), ripple::ttNFTOKEN_CREATE_OFFER},
    {JSL(NFTokenMint), ripple::ttNFTOKEN_MINT},
    {JSL(OfferCancel), ripple::ttOFFER_CANCEL},
    {JSL(OfferCreate), ripple::ttOFFER_CREATE},
    {JSL(Payment), ripple::ttPAYMENT},
    {JSL(PaymentChannelClaim), ripple::ttPAYCHAN_CLAIM},
    {JSL(PaymentChannelCreate), ripple::ttCHECK_CREATE},
    {JSL(PaymentChannelFund), ripple::ttPAYCHAN_FUND},
    {JSL(SetRegularKey), ripple::ttREGULAR_KEY_SET},
    {JSL(SignerListSet), ripple::ttSIGNER_LIST_SET},
    {JSL(TicketCreate), ripple::ttTICKET_CREATE},
    {JSL(TrustSet), ripple::ttTRUST_SET},
};

// TODO: should be std::views::keys when clang supports it
template <typename ETLServiceType>
std::unordered_set<std::string> const BaseAccountTxHandler<ETLServiceType>::TYPES_KEYS = [] {
    std::unordered_set<std::string> keys;
    std::transform(TYPESMAP.begin(), TYPESMAP.end(), std::inserter(keys, keys.begin()), [](auto const& pair) {
        return pair.first;
    });
    return keys;
}();

/**
 * @brief The account_tx method retrieves a list of transactions that involved the specified account.
 *
 * For more details see: https://xrpl.org/account_tx.html
 */
using AccountTxHandler = BaseAccountTxHandler<etl::ETLService>;

}  // namespace rpc
