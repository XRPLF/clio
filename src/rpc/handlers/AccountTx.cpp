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

    if (!request.contains("account"))
        return Status{Error::rpcINVALID_PARAMS, "missingAccount"};

    if (!request.at("account").is_string())
        return Status{Error::rpcINVALID_PARAMS, "accountNotString"};

    auto accountID =
        accountFromStringStrict(request.at("account").as_string().c_str());

    if (!accountID)
        return Status{Error::rpcINVALID_PARAMS, "malformedAccount"};

    bool binary = false;
    if (request.contains("binary"))
    {
        if (!request.at("binary").is_bool())
            return Status{Error::rpcINVALID_PARAMS, "binaryFlagNotBool"};

        binary = request.at("binary").as_bool();
    }
    bool forward = false;
    if (request.contains("forward"))
    {
        if (!request.at("forward").is_bool())
            return Status{Error::rpcINVALID_PARAMS, "forwardNotBool"};

        forward = request.at("forward").as_bool();
    }

    std::optional<Backend::AccountTransactionsCursor> cursor;

    if (request.contains("marker"))
    {
        auto const& obj = request.at("marker").as_object();

        std::optional<std::uint32_t> transactionIndex = {};
        if (obj.contains("seq"))
        {
            if (!obj.at("seq").is_int64())
                return Status{
                    Error::rpcINVALID_PARAMS, "transactionIndexNotInt"};

            transactionIndex =
                boost::json::value_to<std::uint32_t>(obj.at("seq"));
        }

        std::optional<std::uint32_t> ledgerIndex = {};
        if (obj.contains("ledger"))
        {
            if (!obj.at("ledger").is_int64())
                return Status{Error::rpcINVALID_PARAMS, "ledgerIndexNotInt"};

            ledgerIndex =
                boost::json::value_to<std::uint32_t>(obj.at("ledger"));
        }

        if (!transactionIndex || !ledgerIndex)
            return Status{Error::rpcINVALID_PARAMS, "missingLedgerOrSeq"};

        cursor = {*ledgerIndex, *transactionIndex};
    }

    auto minIndex = context.range.minSequence;
    if (request.contains("ledger_index_min"))
    {
        auto& min = request.at("ledger_index_min");

        if (!min.is_int64())
            return Status{Error::rpcINVALID_PARAMS, "ledgerSeqMinNotNumber"};

        if (min.as_int64() != -1)
        {
            if (context.range.maxSequence < min.as_int64() ||
                context.range.minSequence > min.as_int64())
                return Status{
                    Error::rpcINVALID_PARAMS, "ledgerSeqMaxOutOfRange"};
            else
                minIndex = value_to<std::uint32_t>(min);
        }

        if (forward && !cursor)
            cursor = {minIndex, 0};
    }

    auto maxIndex = context.range.maxSequence;
    if (request.contains("ledger_index_max"))
    {
        auto& max = request.at("ledger_index_max");

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

    if (request.contains("ledger_index"))
    {
        if (!request.at("ledger_index").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "ledgerIndexNotNumber"};

        auto ledgerIndex =
            boost::json::value_to<std::uint32_t>(request.at("ledger_index"));
        maxIndex = minIndex = ledgerIndex;
    }

    if (request.contains("ledger_hash"))
    {
        if (!request.at("ledger_hash").is_string())
            return RPC::Status{
                RPC::Error::rpcINVALID_PARAMS, "ledgerHashNotString"};

        ripple::uint256 ledgerHash;
        if (!ledgerHash.parseHex(request.at("ledger_hash").as_string().c_str()))
            return RPC::Status{
                RPC::Error::rpcINVALID_PARAMS, "ledgerHashMalformed"};

        auto lgrInfo =
            context.backend->fetchLedgerByHash(ledgerHash, context.yield);
        maxIndex = minIndex = lgrInfo->seq;
    }

    if (!cursor)
    {
        if (forward)
            cursor = {minIndex, 0};
        else
            cursor = {maxIndex, INT32_MAX};
    }

    std::uint32_t limit = 200;
    if (request.contains("limit"))
    {
        if (!request.at("limit").is_int64())
            return Status{Error::rpcINVALID_PARAMS, "limitNotInt"};

        limit = request.at("limit").as_int64();
        if (limit <= 0)
            return Status{Error::rpcINVALID_PARAMS, "limitNotPositive"};

        response["limit"] = limit;
    }

    boost::json::array txns;
    auto start = std::chrono::system_clock::now();
    auto [blobs, retCursor] = context.backend->fetchAccountTransactions(
        *accountID, limit, forward, cursor, context.yield);

    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info) << __func__ << " db fetch took "
                            << ((end - start).count() / 1000000000.0)
                            << " num blobs = " << blobs.size();

    response["account"] = ripple::to_string(*accountID);

    if (retCursor)
    {
        boost::json::object cursorJson;
        cursorJson["ledger"] = retCursor->ledgerSequence;
        cursorJson["seq"] = retCursor->transactionIndex;
        response["marker"] = cursorJson;
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
            obj["meta"] = meta;
            obj["tx"] = txn;
            obj["tx"].as_object()["ledger_index"] = txnPlusMeta.ledgerSequence;
            obj["tx"].as_object()["date"] = txnPlusMeta.date;
        }
        else
        {
            obj["meta"] = ripple::strHex(txnPlusMeta.metadata);
            obj["tx_blob"] = ripple::strHex(txnPlusMeta.transaction);
            obj["ledger_index"] = txnPlusMeta.ledgerSequence;
            obj["date"] = txnPlusMeta.date;
        }

        txns.push_back(obj);
        if (!minReturnedIndex || txnPlusMeta.ledgerSequence < *minReturnedIndex)
            minReturnedIndex = txnPlusMeta.ledgerSequence;
        if (!maxReturnedIndex || txnPlusMeta.ledgerSequence > *maxReturnedIndex)
            maxReturnedIndex = txnPlusMeta.ledgerSequence;
    }

    assert(cursor);
    if (forward)
    {
        response["ledger_index_min"] = cursor->ledgerSequence;
        if (blobs.size() >= limit)
            response["ledger_index_max"] = *maxReturnedIndex;
        else
            response["ledger_index_max"] = maxIndex;
    }
    else
    {
        response["ledger_index_max"] = cursor->ledgerSequence;
        if (blobs.size() >= limit)
            response["ledger_index_min"] = *minReturnedIndex;
        else
            response["ledger_index_min"] = minIndex;
    }

    response["transactions"] = txns;

    auto end2 = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info) << __func__ << " serialization took "
                            << ((end2 - end).count() / 1000000000.0);

    return response;
}  // namespace RPC

}  // namespace RPC
