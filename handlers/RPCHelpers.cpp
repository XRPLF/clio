#include <handlers/RPCHelpers.h>
#include <reporting/BackendInterface.h>

std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account)
{
    auto blob = ripple::strUnHex(account);

    boost::optional<ripple::PublicKey> publicKey = {};
    if (blob && ripple::publicKeyType(ripple::makeSlice(*blob)))
    {
        publicKey = ripple::PublicKey(
            ripple::Slice{blob->data(), blob->size()});
    }
    else 
    {
        publicKey = ripple::parseBase58<ripple::PublicKey>(
            ripple::TokenType::AccountPublic, account);
    }

    boost::optional<ripple::AccountID> result;
    if (publicKey)
        result = ripple::calcAccountID(*publicKey);
    else
        result = ripple::parseBase58<ripple::AccountID>(account);

    if (result)
        return result.value();
    else
        return {};
}
std::pair<
    std::shared_ptr<ripple::STTx const>,
    std::shared_ptr<ripple::STObject const>>
deserializeTxPlusMeta(Backend::TransactionAndMetadata const& blobs)
{
    std::pair<
        std::shared_ptr<ripple::STTx const>,
        std::shared_ptr<ripple::STObject const>>
        result;
    {
        ripple::SerialIter s{
            blobs.transaction.data(), blobs.transaction.size()};
        result.first = std::make_shared<ripple::STTx const>(s);
    }
    {
        ripple::SerialIter s{blobs.metadata.data(), blobs.metadata.size()};
        result.second =
            std::make_shared<ripple::STObject const>(s, ripple::sfMetadata);
    }
    return result;
}


std::pair<
    std::shared_ptr<ripple::STTx const>,
    std::shared_ptr<ripple::TxMeta const>>
deserializeTxPlusMeta(Backend::TransactionAndMetadata const& blobs, std::uint32_t seq)
{
    auto [tx, meta] = deserializeTxPlusMeta(blobs);

    std::shared_ptr<ripple::TxMeta> m = 
        std::make_shared<ripple::TxMeta>(
            tx->getTransactionID(),
            seq,
            *meta);

    return {tx, m};
}

boost::json::object
getJson(ripple::STBase const& obj)
{
    auto start = std::chrono::system_clock::now();
    boost::json::value value = boost::json::parse(
        obj.getJson(ripple::JsonOptions::none).toStyledString());
    auto end = std::chrono::system_clock::now();
    value.as_object()["deserialization_time_microseconds"] =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    return value.as_object();
}

boost::json::object
getJson(ripple::SLE const& sle)
{
    auto start = std::chrono::system_clock::now();
    boost::json::value value = boost::json::parse(
        sle.getJson(ripple::JsonOptions::none).toStyledString());
    auto end = std::chrono::system_clock::now();
    value.as_object()["deserialization_time_microseconds"] =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    return value.as_object();
}
std::optional<uint32_t>
ledgerSequenceFromRequest(
    boost::json::object const& request,
    BackendInterface const& backend)
{
    if (not request.contains("ledger_index"))
    {
        return backend.fetchLatestLedgerSequence();
    }
    else
    {
        return request.at("ledger_index").as_int64();
    }
}

std::optional<ripple::uint256>
traverseOwnedNodes(
    BackendInterface const& backend,
    ripple::AccountID const& accountID,
    std::uint32_t sequence,
    ripple::uint256 const& cursor,
    std::function<bool(ripple::SLE)> atOwnedNode)
{
    auto const rootIndex = ripple::keylet::ownerDir(accountID);
    auto currentIndex = rootIndex;

    std::vector<ripple::uint256> keys;
    std::optional<ripple::uint256> nextCursor = {};

    auto start = std::chrono::system_clock::now();
    for (;;)
    {
        auto ownedNode =
            backend.fetchLedgerObject(currentIndex.key, sequence);
        
        if (!ownedNode)
        {
            throw std::runtime_error("Could not find owned node");
        }

        ripple::SerialIter it{ownedNode->data(), ownedNode->size()};
        ripple::SLE dir{it, currentIndex.key};

        for (auto const& key : dir.getFieldV256(ripple::sfIndexes))
        {
            if (key >= cursor)
                keys.push_back(key);
        }

        auto const uNodeNext = dir.getFieldU64(ripple::sfIndexNext);
        if (uNodeNext == 0)
            break;

        currentIndex = ripple::keylet::page(rootIndex, uNodeNext);
    }
    auto end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(debug) << "Time loading owned directories: "
                               << ((end - start).count() / 1000000000.0);


    start = std::chrono::system_clock::now();
    auto objects = backend.fetchLedgerObjects(keys, sequence);
    end = std::chrono::system_clock::now();

    BOOST_LOG_TRIVIAL(debug) << "Time loading owned entries: "
                               << ((end - start).count() / 1000000000.0);

    for (auto i = 0; i < objects.size(); ++i)
    {
        ripple::SerialIter it{objects[i].data(), objects[i].size()};
        ripple::SLE sle(it, keys[i]);
        if (!atOwnedNode(sle))
        {
            nextCursor = keys[i+1];
            break;
        }
    }

    return nextCursor;
}
