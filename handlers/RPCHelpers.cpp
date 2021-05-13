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
getJson(ripple::TxMeta const& meta)
{
    auto start = std::chrono::system_clock::now();
    boost::json::value value = boost::json::parse(
        meta.getJson(ripple::JsonOptions::none).toStyledString());
    auto end = std::chrono::system_clock::now();
    value.as_object()["deserialization_time_microseconds"] =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start)
            .count();
    return value.as_object();
}

boost::json::value
getJson(Json::Value const& value)
{
    boost::json::value boostValue = 
        boost::json::parse(value.toStyledString());
    
    return boostValue;
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
