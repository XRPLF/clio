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

#include <rpc/RPCHelpers.h>

#include <set>

namespace RPC {

/**
 * @brief Represents an entry in the book_changes' changes array.
 */
struct BookChange
{
    ripple::STAmount sideAVolume;
    ripple::STAmount sideBVolume;
    ripple::STAmount highRate;
    ripple::STAmount lowRate;
    ripple::STAmount openRate;
    ripple::STAmount closeRate;
};

/**
 * @brief Encapsulates the book_changes computations and transformations.
 */
class BookChanges final
{
public:
    BookChanges() = delete;  // only accessed via static handle function

    /**
     * @brief Computes all book_changes for the given transactions.
     *
     * @param transactions The transactions to compute book changes for
     * @return std::vector<BookChange> Book changes
     */
    [[nodiscard]] static std::vector<BookChange>
    compute(std::vector<Backend::TransactionAndMetadata> const& transactions)
    {
        return HandlerImpl{}(transactions);
    }

private:
    class HandlerImpl final
    {
        std::map<std::string, BookChange> tally_ = {};
        std::optional<uint32_t> offerCancel_ = {};

    public:
        [[nodiscard]] std::vector<BookChange>
        operator()(std::vector<Backend::TransactionAndMetadata> const& transactions)
        {
            for (auto const& tx : transactions)
                handleBookChange(tx);

            // TODO: rewrite this with std::ranges when compilers catch up
            std::vector<BookChange> changes;
            std::transform(
                std::make_move_iterator(std::begin(tally_)),
                std::make_move_iterator(std::end(tally_)),
                std::back_inserter(changes),
                [](auto obj) { return obj.second; });
            return changes;
        }

    private:
        void
        handleAffectedNode(ripple::STObject const& node)
        {
            auto const& metaType = node.getFName();
            auto const nodeType = node.getFieldU16(ripple::sfLedgerEntryType);

            // we only care about ripple::ltOFFER objects being modified or
            // deleted
            if (nodeType != ripple::ltOFFER || metaType == ripple::sfCreatedNode)
                return;

            // if either FF or PF are missing we can't compute
            // but generally these are cancelled rather than crossed
            // so skipping them is consistent
            if (!node.isFieldPresent(ripple::sfFinalFields) || !node.isFieldPresent(ripple::sfPreviousFields))
                return;

            auto const& finalFields = node.peekAtField(ripple::sfFinalFields).downcast<ripple::STObject>();
            auto const& previousFields = node.peekAtField(ripple::sfPreviousFields).downcast<ripple::STObject>();

            // defensive case that should never be hit
            if (!finalFields.isFieldPresent(ripple::sfTakerGets) || !finalFields.isFieldPresent(ripple::sfTakerPays) ||
                !previousFields.isFieldPresent(ripple::sfTakerGets) ||
                !previousFields.isFieldPresent(ripple::sfTakerPays))
                return;

            // filter out any offers deleted by explicit offer cancels
            if (metaType == ripple::sfDeletedNode && offerCancel_ &&
                finalFields.getFieldU32(ripple::sfSequence) == *offerCancel_)
                return;

            // compute the difference in gets and pays actually
            // affected onto the offer
            auto const deltaGets =
                finalFields.getFieldAmount(ripple::sfTakerGets) - previousFields.getFieldAmount(ripple::sfTakerGets);
            auto const deltaPays =
                finalFields.getFieldAmount(ripple::sfTakerPays) - previousFields.getFieldAmount(ripple::sfTakerPays);

            transformAndStore(deltaGets, deltaPays);
        }

        void
        transformAndStore(ripple::STAmount const& deltaGets, ripple::STAmount const& deltaPays)
        {
            auto const g = to_string(deltaGets.issue());
            auto const p = to_string(deltaPays.issue());

            auto const noswap = isXRP(deltaGets) ? true : (isXRP(deltaPays) ? false : (g < p));

            auto first = noswap ? deltaGets : deltaPays;
            auto second = noswap ? deltaPays : deltaGets;

            // defensively programmed, should (probably) never happen
            if (second == beast::zero)
                return;

            auto const rate = divide(first, second, ripple::noIssue());

            if (first < beast::zero)
                first = -first;

            if (second < beast::zero)
                second = -second;

            auto const key = noswap ? (g + '|' + p) : (p + '|' + g);
            if (tally_.contains(key))
            {
                auto& entry = tally_.at(key);

                entry.sideAVolume += first;
                entry.sideBVolume += second;

                if (entry.highRate < rate)
                    entry.highRate = rate;

                if (entry.lowRate > rate)
                    entry.lowRate = rate;

                entry.closeRate = rate;
            }
            else
            {
                tally_[key] = {
                    .sideAVolume = first,
                    .sideBVolume = second,
                    .highRate = rate,
                    .lowRate = rate,
                    .openRate = rate,
                    .closeRate = rate,
                };
            }
        }

        void
        handleBookChange(Backend::TransactionAndMetadata const& blob)
        {
            auto const [tx, meta] = RPC::deserializeTxPlusMeta(blob);
            if (!tx || !meta || !tx->isFieldPresent(ripple::sfTransactionType))
                return;

            offerCancel_ = shouldCancelOffer(tx);
            for (auto const& node : meta->getFieldArray(ripple::sfAffectedNodes))
                handleAffectedNode(node);
        }

        std::optional<uint32_t>
        shouldCancelOffer(std::shared_ptr<ripple::STTx const> const& tx) const
        {
            switch (tx->getFieldU16(ripple::sfTransactionType))
            {
                // in future if any other ways emerge to cancel an offer
                // this switch makes them easy to add
                case ripple::ttOFFER_CANCEL:
                case ripple::ttOFFER_CREATE:
                    if (tx->isFieldPresent(ripple::sfOfferSequence))
                        return tx->getFieldU32(ripple::sfOfferSequence);
                default:
                    return std::nullopt;
            }
        }
    };
};

inline void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, BookChange const& change)
{
    auto amountStr = [](ripple::STAmount const& amount) -> std::string {
        return isXRP(amount) ? to_string(amount.xrp()) : to_string(amount.iou());
    };

    auto currencyStr = [](ripple::STAmount const& amount) -> std::string {
        return isXRP(amount) ? "XRP_drops" : to_string(amount.issue());
    };

    jv = {
        {JS(currency_a), currencyStr(change.sideAVolume)},
        {JS(currency_b), currencyStr(change.sideBVolume)},
        {JS(volume_a), amountStr(change.sideAVolume)},
        {JS(volume_b), amountStr(change.sideBVolume)},
        {JS(high), to_string(change.highRate.iou())},
        {JS(low), to_string(change.lowRate.iou())},
        {JS(open), to_string(change.openRate.iou())},
        {JS(close), to_string(change.closeRate.iou())},
    };
}

[[nodiscard]] boost::json::object const
computeBookChanges(
    ripple::LedgerHeader const& lgrInfo,
    std::vector<Backend::TransactionAndMetadata> const& transactions);

}  // namespace RPC
