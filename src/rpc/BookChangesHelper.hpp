/** @file */
#pragma once

#include "data/Types.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <xrpl/beast/utility/Zero.h>
#include <xrpl/protocol/IOUAmount.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/XRPAmount.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace rpc {

/**
 * @brief Represents an entry in the book_changes' changes array.
 */
struct BookChange {
    xrpl::STAmount sideAVolume;
    xrpl::STAmount sideBVolume;
    xrpl::STAmount highRate;
    xrpl::STAmount lowRate;
    xrpl::STAmount openRate;
    xrpl::STAmount closeRate;
    std::optional<xrpl::uint256> domain;
};

/**
 * @brief Encapsulates the book_changes computations and transformations.
 */
class BookChanges final {
public:
    BookChanges() = delete;  // only accessed via static handle function

    /**
     * @brief Computes all book_changes for the given transactions.
     *
     * @param transactions The transactions to compute book changes for
     * @return Book changes
     */
    [[nodiscard]] static std::vector<BookChange>
    compute(std::vector<data::TransactionAndMetadata> const& transactions)
    {
        return HandlerImpl{}(transactions);
    }

private:
    class HandlerImpl final {
        std::map<std::string, BookChange> tally_;
        std::optional<uint32_t> offerCancel_;

    public:
        [[nodiscard]] std::vector<BookChange>
        operator()(std::vector<data::TransactionAndMetadata> const& transactions)
        {
            for (auto const& tx : transactions)
                handleBookChange(tx);

            // TODO: rewrite this with std::ranges when compilers catch up
            std::vector<BookChange> changes;
            std::transform(
                std::make_move_iterator(std::begin(tally_)),
                std::make_move_iterator(std::end(tally_)),
                std::back_inserter(changes),
                [](auto obj) { return obj.second; }
            );
            return changes;
        }

    private:
        void
        handleAffectedNode(xrpl::STObject const& node)
        {
            auto const& metaType = node.getFName();
            auto const nodeType = node.getFieldU16(xrpl::sfLedgerEntryType);

            // we only care about xrpl::ltOFFER objects being modified or
            // deleted
            if (nodeType != xrpl::ltOFFER || metaType == xrpl::sfCreatedNode)
                return;

            // if either FF or PF are missing we can't compute
            // but generally these are cancelled rather than crossed
            // so skipping them is consistent
            if (!node.isFieldPresent(xrpl::sfFinalFields) ||
                !node.isFieldPresent(xrpl::sfPreviousFields))
                return;

            auto const& finalFields =
                node.peekAtField(xrpl::sfFinalFields).downcast<xrpl::STObject>();
            auto const& previousFields =
                node.peekAtField(xrpl::sfPreviousFields).downcast<xrpl::STObject>();

            // defensive case that should never be hit
            if (!finalFields.isFieldPresent(xrpl::sfTakerGets) ||
                !finalFields.isFieldPresent(xrpl::sfTakerPays) ||
                !previousFields.isFieldPresent(xrpl::sfTakerGets) ||
                !previousFields.isFieldPresent(xrpl::sfTakerPays))
                return;

            // filter out any offers deleted by explicit offer cancels
            if (metaType == xrpl::sfDeletedNode && offerCancel_ &&
                finalFields.getFieldU32(xrpl::sfSequence) == *offerCancel_)
                return;

            // compute the difference in gets and pays actually
            // affected onto the offer
            auto const deltaGets = finalFields.getFieldAmount(xrpl::sfTakerGets) -
                previousFields.getFieldAmount(xrpl::sfTakerGets);
            auto const deltaPays = finalFields.getFieldAmount(xrpl::sfTakerPays) -
                previousFields.getFieldAmount(xrpl::sfTakerPays);

            transformAndStore(deltaGets, deltaPays, finalFields[~xrpl::sfDomainID]);
        }

        void
        transformAndStore(
            xrpl::STAmount const& deltaGets,
            xrpl::STAmount const& deltaPays,
            std::optional<xrpl::uint256> const& domain
        )
        {
            auto const g = to_string(deltaGets.get<xrpl::Issue>());
            auto const p = to_string(deltaPays.get<xrpl::Issue>());

            auto const noswap = [&]() {
                if (isXRP(deltaGets))
                    return true;
                return isXRP(deltaPays) ? false : (g < p);
            }();

            auto first = noswap ? deltaGets : deltaPays;
            auto second = noswap ? deltaPays : deltaGets;

            // defensively programmed, should (probably) never happen
            if (second == beast::kZero)
                return;

            auto const rate = divide(first, second, xrpl::noIssue());

            if (first < beast::kZero)
                first = -first;

            if (second < beast::kZero)
                second = -second;

            auto const key = noswap ? (g + '|' + p) : (p + '|' + g);
            if (tally_.contains(key)) {
                auto& entry = tally_.at(key);

                entry.sideAVolume += first;
                entry.sideBVolume += second;

                if (entry.highRate < rate)
                    entry.highRate = rate;

                if (entry.lowRate > rate)
                    entry.lowRate = rate;

                entry.closeRate = rate;
                entry.domain = domain;
            } else {
                tally_[key] = {
                    .sideAVolume = first,
                    .sideBVolume = second,
                    .highRate = rate,
                    .lowRate = rate,
                    .openRate = rate,
                    .closeRate = rate,
                    .domain = domain,
                };
            }
        }

        void
        handleBookChange(data::TransactionAndMetadata const& blob)
        {
            auto const [tx, meta] = rpc::deserializeTxPlusMeta(blob);
            if (!tx || !meta || !tx->isFieldPresent(xrpl::sfTransactionType))
                return;

            offerCancel_ = shouldCancelOffer(tx);
            for (auto const& node : meta->getFieldArray(xrpl::sfAffectedNodes))
                handleAffectedNode(node);
        }

        static std::optional<uint32_t>
        shouldCancelOffer(std::shared_ptr<xrpl::STTx const> const& tx)
        {
            switch (tx->getFieldU16(xrpl::sfTransactionType)) {
                // in future if any other ways emerge to cancel an offer
                // this switch makes them easy to add
                case xrpl::ttOFFER_CANCEL:
                case xrpl::ttOFFER_CREATE:
                    if (tx->isFieldPresent(xrpl::sfOfferSequence))
                        return tx->getFieldU32(xrpl::sfOfferSequence);
                    [[fallthrough]];
                default:
                    return std::nullopt;
            }
        }
    };
};

/**
 * @brief Implementation of value_from for BookChange type.
 *
 * @param [out] jv The JSON value to populate
 * @param change The BookChange to serialize
 */
inline void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, BookChange const& change)
{
    auto amountStr = [](xrpl::STAmount const& amount) -> std::string {
        return isXRP(amount) ? to_string(amount.xrp()) : to_string(amount.iou());
    };

    auto currencyStr = [](xrpl::STAmount const& amount) -> std::string {
        return isXRP(amount) ? "XRP_drops" : to_string(amount.get<xrpl::Issue>());
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

    if (change.domain.has_value())
        jv.as_object()[JS(domain)] = xrpl::to_string(*change.domain);
}

/**
 * @brief Computes all book changes for the given ledger header and transactions.
 *
 * @param lgrInfo The ledger header
 * @param transactions The vector of transactions with heir metadata
 * @return The book changes
 */
[[nodiscard]] boost::json::object
computeBookChanges(
    xrpl::LedgerHeader const& lgrInfo,
    std::vector<data::TransactionAndMetadata> const& transactions
);

}  // namespace rpc
