#include <backend/BackendInterface.h>
#include <backend/Pg.h>
#include <rpc/RPCHelpers.h>

namespace RPC {

using boost::json::value_to;

Result
doAccountTx(Context const& context)
{
    auto request = context.params;
    boost::json::object response = {};

    ripple::AccountID accountID;
    if (auto const status = getAccount(request, accountID); status)
        return status;

    bool const binary = getBool(request, JS(binary), false);
    bool const forward = getBool(request, JS(forward), false);

    std::optional<Backend::AccountTransactionsCursor> cursor;

    if (request.contains(JS(marker)))
    {
        auto const& obj = request.at(JS(marker)).as_object();

        std::optional<std::uint32_t> transactionIndex = {};
        if (obj.contains(JS(seq)))
        {
            if (!obj.at(JS(seq)).is_int64())
                return Status{
                    Error::rpcINVALID_PARAMS, "transactionIndexNotInt"};

            transactionIndex =
                boost::json::value_to<std::uint32_t>(obj.at(JS(seq)));
        }

        std::optional<std::uint32_t> ledgerIndex = {};
        if (obj.contains(JS(ledger)))
        {
            if (!obj.at(JS(ledger)).is_int64())
                return Status{Error::rpcINVALID_PARAMS, "ledgerIndexNotInt"};

            ledgerIndex =
                boost::json::value_to<std::uint32_t>(obj.at(JS(ledger)));
        }

        if (!transactionIndex || !ledgerIndex)
            return Status{Error::rpcINVALID_PARAMS, "missingLedgerOrSeq"};

        cursor = {*ledgerIndex, *transactionIndex};
    }

    auto minIndex = context.range.minSequence;
    if (request.contains(JS(ledger_index_min)))
    {
        auto& min = request.at(JS(ledger_index_min));

        if (!min.is_int64())
            return Status{Error::rpcINVALID_PARAMS, "ledgerSeqMinNotNumber"};

        if (min.as_int64() != -1)
        {
            if (context.range.maxSequence < min.as_int64() ||
                context.range.minSequence > min.as_int64())
                return Status{
                    Error::rpcINVALID_PARAMS, "ledgerSeqMinOutOfRange"};
            else
                minIndex = value_to<std::uint32_t>(min);
        }

        if (forward && !cursor)
            cursor = {minIndex, 0};
    }

    auto maxIndex = context.range.maxSequence;
    if (request.contains(JS(ledger_index_max)))
    {
        auto& max = request.at(JS(ledger_index_max));

        if (!max.is_int64())
            return Status{Error::rpcINVALID_PARAMS, "ledgerSeqMaxNotNumber"};

        if (max.as_int64() != -1)
        {
            if (context.range.maxSequence < max.as_int64() ||
                context.range.minSequence > max.as_int64())
                return Status{
                    Error::rpcINVALID_PARAMS, "ledgerSeqMaxOutOfRange"};
            else
                maxIndex = value_to<std::uint32_t>(max);
        }

        if (minIndex > maxIndex)
            return Status{Error::rpcINVALID_PARAMS, "invalidIndex"};

        if (!forward && !cursor)
            cursor = {maxIndex, INT32_MAX};
    }

    if (request.contains(JS(ledger_index)) || request.contains(JS(ledger_hash)))
    {
        if (request.contains(JS(ledger_index_max)) ||
            request.contains(JS(ledger_index_min)))
            return Status{
                Error::rpcINVALID_PARAMS, "containsLedgerSpecifierAndRange"};

        auto v = ledgerInfoFromRequest(context);
        if (auto status = std::get_if<Status>(&v))
            return *status;

        maxIndex = minIndex = std::get<ripple::LedgerInfo>(v).seq;
    }

    if (!cursor)
    {
        if (forward)
            cursor = {minIndex, 0};
        else
            cursor = {maxIndex, INT32_MAX};
    }

    std::uint32_t limit = 200;
    if (auto const status = getLimit(request, limit); status)
        return status;

    if (request.contains(JS(limit)))
        response[JS(limit)] = limit;

    boost::json::array txns;
    auto start = std::chrono::system_clock::now();
    auto [blobs, retCursor] = context.backend->fetchAccountTransactions(
        accountID, limit, forward, cursor, context.yield);

    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info) << __func__ << " db fetch took "
                            << ((end - start).count() / 1000000000.0)
                            << " num blobs = " << blobs.size();

    response[JS(account)] = ripple::to_string(accountID);

    if (retCursor)
    {
        boost::json::object cursorJson;
        cursorJson[JS(ledger)] = retCursor->ledgerSequence;
        cursorJson[JS(seq)] = retCursor->transactionIndex;
        response[JS(marker)] = cursorJson;
    }

    std::optional<size_t> maxReturnedIndex;
    std::optional<size_t> minReturnedIndex;
    for (auto const& txnPlusMeta : blobs)
    {
        if (txnPlusMeta.ledgerSequence < minIndex ||
            txnPlusMeta.ledgerSequence > maxIndex)
        {
            BOOST_LOG_TRIVIAL(debug)
                << __func__
                << " skipping over transactions from incomplete ledger";
            continue;
        }

        boost::json::object obj;

        if (!binary)
        {
            auto [txn, meta] = toExpandedJson(txnPlusMeta);
            obj[JS(meta)] = meta;
            obj[JS(tx)] = txn;
            obj[JS(tx)].as_object()[JS(ledger_index)] =
                txnPlusMeta.ledgerSequence;
            obj[JS(tx)].as_object()[JS(date)] = txnPlusMeta.date;
        }
        else
        {
            obj[JS(meta)] = ripple::strHex(txnPlusMeta.metadata);
            obj[JS(tx_blob)] = ripple::strHex(txnPlusMeta.transaction);
            obj[JS(ledger_index)] = txnPlusMeta.ledgerSequence;
            obj[JS(date)] = txnPlusMeta.date;
        }
        obj[JS(validated)] = true;

        txns.push_back(obj);
        if (!minReturnedIndex || txnPlusMeta.ledgerSequence < *minReturnedIndex)
            minReturnedIndex = txnPlusMeta.ledgerSequence;
        if (!maxReturnedIndex || txnPlusMeta.ledgerSequence > *maxReturnedIndex)
            maxReturnedIndex = txnPlusMeta.ledgerSequence;
    }

    assert(cursor);
    if (!forward)
    {
        response[JS(ledger_index_min)] = cursor->ledgerSequence;
        response[JS(ledger_index_max)] = maxIndex;
    }
    else
    {
        response[JS(ledger_index_max)] = cursor->ledgerSequence;
        response[JS(ledger_index_min)] = minIndex;
    }

    response[JS(transactions)] = txns;

    auto end2 = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info) << __func__ << " serialization took "
                            << ((end2 - end).count() / 1000000000.0);

    return response;
}  // namespace RPC

}  // namespace RPC
