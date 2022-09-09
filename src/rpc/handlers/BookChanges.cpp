#include <ripple/app/ledger/Ledger.h>
#include <ripple/basics/ToString.h>

#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>

#include <boost/json.hpp>
#include <algorithm>

namespace json = boost::json;
using namespace ripple;

namespace RPC {

struct BookChange
{
    STAmount sideAVolume;
    STAmount sideBVolume;
    STAmount highRate;
    STAmount lowRate;
    STAmount openRate;
    STAmount closeRate;
};

class BookChangesHandler
{
    std::reference_wrapper<Context const> context_;
    std::map<std::string, BookChange> tally_;
    std::optional<uint32_t> offerCancel_;

public:
    ~BookChangesHandler() = default;
    BookChangesHandler(Context const& context) : context_{std::cref(context)}
    {
    }

    BookChangesHandler(BookChangesHandler const&) = delete;
    BookChangesHandler(BookChangesHandler&&) = delete;
    BookChangesHandler&
    operator=(BookChangesHandler const&) = delete;
    BookChangesHandler&
    operator=(BookChangesHandler&&) = delete;

    /**
     * @brief Handles the `book_change` request for given transactions
     *
     * @param transactions The transactions to compute changes for
     * @return std::vector<BookChange> The changes
     */
    std::vector<BookChange>
    handle(LedgerInfo const& ledger)
    {
        reset();

        auto const transactions =
            context_.get().backend->fetchAllTransactionsInLedger(
                ledger.seq, context_.get().yield);
        for (auto const& tx : transactions)
            handleBookChange(tx);

        std::vector<BookChange> changes;
        std::transform(
            std::make_move_iterator(std::begin(tally_)),
            std::make_move_iterator(std::end(tally_)),
            std::back_inserter(changes),
            [](auto obj) { return obj.second; });
        return changes;
    }

private:
    inline void
    reset()
    {
        tally_.clear();
        offerCancel_ = std::nullopt;
    }

    void
    handleAffectedNode(STObject const& node)
    {
        auto const& metaType = node.getFName();
        auto const nodeType = node.getFieldU16(sfLedgerEntryType);

        // we only care about ltOFFER objects being modified or
        // deleted
        if (nodeType != ltOFFER || metaType == sfCreatedNode)
            return;

        // if either FF or PF are missing we can't compute
        // but generally these are cancelled rather than crossed
        // so skipping them is consistent
        if (!node.isFieldPresent(sfFinalFields) ||
            !node.isFieldPresent(sfPreviousFields))
            return;

        auto& finalFields = (const_cast<STObject&>(node))
                                .getField(sfFinalFields)
                                .downcast<STObject>();

        auto& previousFields = (const_cast<STObject&>(node))
                                   .getField(sfPreviousFields)
                                   .downcast<STObject>();

        // defensive case that should never be hit
        if (!finalFields.isFieldPresent(sfTakerGets) ||
            !finalFields.isFieldPresent(sfTakerPays) ||
            !previousFields.isFieldPresent(sfTakerGets) ||
            !previousFields.isFieldPresent(sfTakerPays))
            return;

        // filter out any offers deleted by explicit offer cancels
        if (metaType == sfDeletedNode && offerCancel_ &&
            finalFields.getFieldU32(sfSequence) == *offerCancel_)
            return;

        // compute the difference in gets and pays actually
        // affected onto the offer
        auto const deltaGets = finalFields.getFieldAmount(sfTakerGets) -
            previousFields.getFieldAmount(sfTakerGets);
        auto const deltaPays = finalFields.getFieldAmount(sfTakerPays) -
            previousFields.getFieldAmount(sfTakerPays);

        auto const g = to_string(deltaGets.issue());
        auto const p = to_string(deltaPays.issue());

        auto const noswap =
            isXRP(deltaGets) ? true : (isXRP(deltaPays) ? false : (g < p));

        auto first = noswap ? deltaGets : deltaPays;
        auto second = noswap ? deltaPays : deltaGets;

        // defensively programmed, should (probably) never happen
        if (second == beast::zero)
            return;

        auto const rate = divide(first, second, noIssue());

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
                first,   // side A vol
                second,  // side B vol
                rate,    // high
                rate,    // low
                rate,    // open
                rate     // close
            };
        }
    }

    void
    handleBookChange(Backend::TransactionAndMetadata const& blob)
    {
        auto const [tx, meta] = deserializeTxPlusMeta(blob);
        if (!tx || !meta || !tx->isFieldPresent(sfTransactionType))
            return;

        offerCancel_ = shouldCancelOffer(tx);
        for (auto const& node : meta->getFieldArray(sfAffectedNodes))
            handleAffectedNode(node);
    }

    std::optional<uint32_t>
    shouldCancelOffer(std::shared_ptr<ripple::STTx const> const& tx)
    {
        switch (tx->getFieldU16(sfTransactionType))
        {
            // in future if any other ways emerge to cancel an offer
            // this switch makes them easy to add
            case ttOFFER_CANCEL:
            case ttOFFER_CREATE:
                if (tx->isFieldPresent(sfOfferSequence))
                    return tx->getFieldU32(sfOfferSequence);
            default:
                return std::nullopt;
        }
    }
};

void
tag_invoke(
    const json::value_from_tag&,
    json::value& jv,
    BookChange const& change)
{
    auto amountStr = [](STAmount const& amount) -> std::string {
        return isXRP(amount) ? to_string(amount.xrp())
                             : to_string(amount.iou());
    };

    auto currencyStr = [](STAmount const& amount) -> std::string {
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

Result
doBookChanges(Context const& context)
{
    auto const request = context.params;
    auto const v = ledgerInfoFromRequest(context);
    if (auto const status = std::get_if<Status>(&v))
        return *status;

    auto const lgrInfo = std::get<ripple::LedgerInfo>(v);
    auto const changes = BookChangesHandler{context}.handle(lgrInfo);
    auto response = json::object{};

    response[JS(type)] = "bookChanges";
    response[JS(ledger_index)] = lgrInfo.seq;
    response[JS(ledger_hash)] = to_string(lgrInfo.hash);
    response[JS(ledger_time)] = lgrInfo.closeTime.time_since_epoch().count();
    response[JS(changes)] = json::value_from(changes);

    return response;
}

}  // namespace RPC
