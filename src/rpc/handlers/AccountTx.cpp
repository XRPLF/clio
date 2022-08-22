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

    std::optional<Backend::TransactionsCursor> cursor;

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

    std::uint32_t limit;
    if (auto const status = getLimit(context, limit); status)
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

    //@fmendoz7, 08/08 1152 PST | Cancels extraneous cursor on last page (deleted in light of new changes)
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
        /* 
            TODO: Split into TWO, append additional logic. We should make changes here
                > No cursor for case1 & case2
                > [!!!] Take into account WHICH DIRECTION you're going so you don't erroneously flag no return, using forward bool
        */
        //CASE #1: [ABNORMAL] Moving to the LHS, less than minIndex
        if (txnPlusMeta.ledgerSequence < minIndex && !forward) {
            BOOST_LOG_TRIVIAL(debug)
                << __func__
                << " skipping over transactions from incomplete ledger";
            continue;
        }

        //CASE #2: [ABNORMAL] Moving to RHS, greater than maxIndex
        if(txnPlusMeta.ledgerSequence > maxIndex && forward) {
            BOOST_LOG_TRIVIAL(debug)
                << __func__
                << " skipping over transactions from incomplete ledger";
            continue;
        }

        //CASE #3: [NORMAL] Moving to LHS (BACKWARD), greater than minIndex
        //CASE #4: [NORMAL] Moving to RHS (FORWARD), less than maxIndex

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

        /*
            Just because we skip txns doesn't mean we're finished iterating
                >> ARCHETYPE: if txns.size() <= limit && {case1 || case2}, don't return cursor
                > CASE #1: If going BACKWARD and < minIndex, DON'T RETURN CURSOR
                > CASE #2: If going FORWARD and > maxIndex, DON'T RETURN CURSOR
                > CASE #3 + #4: Case 1 + Case 2, but take into account which direction you're going w. forward bool. RETURN CURSOR if not done 
        */
        txns.push_back(obj);

        //CASE #3: Before LHS limit of window, but iterating FORWARD
        if (!minReturnedIndex || (txnPlusMeta.ledgerSequence < *minReturnedIndex && forward)) {
            minReturnedIndex = txnPlusMeta.ledgerSequence;
        }
        //CASE #4: After RHS limit of window, but iterating BACKWARD
        if (!maxReturnedIndex || (txnPlusMeta.ledgerSequence > *maxReturnedIndex && !forward)) {
            maxReturnedIndex = txnPlusMeta.ledgerSequence;
        }
        //CASE #1 + #2: TODO
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