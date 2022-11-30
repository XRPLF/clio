#include <ripple/basics/StringUtilities.h>
#include <backend/BackendInterface.h>
#include <log/Logger.h>
#include <rpc/RPCHelpers.h>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

using namespace clio;

// local to compilation unit loggers
namespace {
clio::Logger gLog{"RPC"};
}  // namespace

namespace RPC {

std::optional<bool>
getBool(boost::json::object const& request, std::string const& field)
{
    if (!request.contains(field))
        return {};
    else if (request.at(field).is_bool())
        return request.at(field).as_bool();
    else
        throw InvalidParamsError("Invalid field " + field + ", not bool.");
}

bool
getBool(
    boost::json::object const& request,
    std::string const& field,
    bool dfault)
{
    if (auto res = getBool(request, field))
        return *res;
    else
        return dfault;
}

bool
getRequiredBool(boost::json::object const& request, std::string const& field)
{
    if (auto res = getBool(request, field))
        return *res;
    else
        throw InvalidParamsError("Missing field " + field);
}

std::optional<std::uint32_t>
getUInt(boost::json::object const& request, std::string const& field)
{
    if (!request.contains(field))
        return {};
    else if (request.at(field).is_uint64())
        return request.at(field).as_uint64();
    else if (request.at(field).is_int64())
        return request.at(field).as_int64();
    else
        throw InvalidParamsError("Invalid field " + field + ", not uint.");
}

std::uint32_t
getUInt(
    boost::json::object const& request,
    std::string const& field,
    std::uint32_t const dfault)
{
    if (auto res = getUInt(request, field))
        return *res;
    else
        return dfault;
}

std::uint32_t
getRequiredUInt(boost::json::object const& request, std::string const& field)
{
    if (auto res = getUInt(request, field))
        return *res;
    else
        throw InvalidParamsError("Missing field " + field);
}

std::optional<AccountCursor>
parseAccountCursor(
    BackendInterface const& backend,
    std::uint32_t seq,
    std::optional<std::string> jsonCursor,
    boost::asio::yield_context& yield)
{
    ripple::uint256 cursorIndex = beast::zero;
    std::uint64_t startHint = 0;

    if (!jsonCursor)
        return AccountCursor({cursorIndex, startHint});

    // Cursor is composed of a comma separated index and start hint. The
    // former will be read as hex, and the latter using boost lexical cast.
    std::stringstream cursor(*jsonCursor);
    std::string value;
    if (!std::getline(cursor, value, ','))
        return {};

    if (!cursorIndex.parseHex(value))
        return {};

    if (!std::getline(cursor, value, ','))
        return {};

    try
    {
        startHint = boost::lexical_cast<std::uint64_t>(value);
    }
    catch (boost::bad_lexical_cast&)
    {
        return {};
    }

    return AccountCursor({cursorIndex, startHint});
}

std::optional<std::string>
getString(boost::json::object const& request, std::string const& field)
{
    if (!request.contains(field))
        return {};
    else if (request.at(field).is_string())
        return request.at(field).as_string().c_str();
    else
        throw InvalidParamsError("Invalid field " + field + ", not string.");
}

std::string
getRequiredString(boost::json::object const& request, std::string const& field)
{
    if (auto res = getString(request, field))
        return *res;
    else
        throw InvalidParamsError("Missing field " + field);
}

std::string
getString(
    boost::json::object const& request,
    std::string const& field,
    std::string dfault)
{
    if (auto res = getString(request, field))
        return *res;
    else
        return dfault;
}

Status
getHexMarker(boost::json::object const& request, ripple::uint256& marker)
{
    if (request.contains(JS(marker)))
    {
        if (!request.at(JS(marker)).is_string())
            return Status{RippledError::rpcINVALID_PARAMS, "markerNotString"};

        if (!marker.parseHex(request.at(JS(marker)).as_string().c_str()))
            return Status{RippledError::rpcINVALID_PARAMS, "malformedMarker"};
    }

    return {};
}

Status
getAccount(
    boost::json::object const& request,
    ripple::AccountID& account,
    boost::string_view const& field,
    bool required)
{
    if (!request.contains(field))
    {
        if (required)
            return Status{
                RippledError::rpcINVALID_PARAMS, field.to_string() + "Missing"};

        return {};
    }

    if (!request.at(field).is_string())
        return Status{
            RippledError::rpcINVALID_PARAMS, field.to_string() + "NotString"};

    if (auto a = accountFromStringStrict(request.at(field).as_string().c_str());
        a)
    {
        account = a.value();
        return {};
    }

    return Status{
        RippledError::rpcACT_MALFORMED, field.to_string() + "Malformed"};
}

Status
getOptionalAccount(
    boost::json::object const& request,
    std::optional<ripple::AccountID>& account,
    boost::string_view const& field)
{
    if (!request.contains(field))
    {
        account = {};
        return {};
    }

    if (!request.at(field).is_string())
        return Status{
            RippledError::rpcINVALID_PARAMS, field.to_string() + "NotString"};

    if (auto a = accountFromStringStrict(request.at(field).as_string().c_str());
        a)
    {
        account = a.value();
        return {};
    }

    return Status{
        RippledError::rpcINVALID_PARAMS, field.to_string() + "Malformed"};
}

Status
getAccount(boost::json::object const& request, ripple::AccountID& accountId)
{
    return getAccount(request, accountId, JS(account), true);
}

Status
getAccount(
    boost::json::object const& request,
    ripple::AccountID& destAccount,
    boost::string_view const& field)
{
    return getAccount(request, destAccount, field, false);
}

Status
getTaker(boost::json::object const& request, ripple::AccountID& takerID)
{
    if (request.contains(JS(taker)))
    {
        auto parsed = parseTaker(request.at(JS(taker)));
        if (auto status = std::get_if<Status>(&parsed); status)
            return *status;
        else
            takerID = std::get<ripple::AccountID>(parsed);
    }

    return {};
}

Status
getChannelId(boost::json::object const& request, ripple::uint256& channelId)
{
    if (!request.contains(JS(channel_id)))
        return Status{RippledError::rpcINVALID_PARAMS, "missingChannelID"};

    if (!request.at(JS(channel_id)).is_string())
        return Status{RippledError::rpcINVALID_PARAMS, "channelIDNotString"};

    if (!channelId.parseHex(request.at(JS(channel_id)).as_string().c_str()))
        return Status{RippledError::rpcCHANNEL_MALFORMED, "malformedChannelID"};

    return {};
}

std::optional<ripple::STAmount>
getDeliveredAmount(
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta,
    std::uint32_t const ledgerSequence,
    uint32_t date)
{
    if (meta->hasDeliveredAmount())
        return meta->getDeliveredAmount();
    if (txn->isFieldPresent(ripple::sfAmount))
    {
        using namespace std::chrono_literals;

        // Ledger 4594095 is the first ledger in which the DeliveredAmount field
        // was present when a partial payment was made and its absence indicates
        // that the amount delivered is listed in the Amount field.
        //
        // If the ledger closed long after the DeliveredAmount code was deployed
        // then its absence indicates that the amount delivered is listed in the
        // Amount field. DeliveredAmount went live January 24, 2014.
        // 446000000 is in Feb 2014, well after DeliveredAmount went live
        if (ledgerSequence >= 4594095 || date > 446000000)
        {
            return txn->getFieldAmount(ripple::sfAmount);
        }
    }
    return {};
}

bool
canHaveDeliveredAmount(
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta)
{
    ripple::TxType const tt{txn->getTxnType()};
    if (tt != ripple::ttPAYMENT && tt != ripple::ttCHECK_CASH &&
        tt != ripple::ttACCOUNT_DELETE)
        return false;

    /*
    if (tt == ttCHECK_CASH && !getFix1623Enabled())
        return false;
        */

    if (meta->getResultTER() != ripple::tesSUCCESS)
        return false;

    return true;
}

std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account)
{
    auto blob = ripple::strUnHex(account);

    std::optional<ripple::PublicKey> publicKey = {};
    if (blob && ripple::publicKeyType(ripple::makeSlice(*blob)))
    {
        publicKey =
            ripple::PublicKey(ripple::Slice{blob->data(), blob->size()});
    }
    else
    {
        publicKey = ripple::parseBase58<ripple::PublicKey>(
            ripple::TokenType::AccountPublic, account);
    }

    std::optional<ripple::AccountID> result;
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
    try
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
    catch (std::exception const& e)
    {
        std::stringstream txn;
        std::stringstream meta;
        std::copy(
            blobs.transaction.begin(),
            blobs.transaction.end(),
            std::ostream_iterator<unsigned char>(txn));
        std::copy(
            blobs.metadata.begin(),
            blobs.metadata.end(),
            std::ostream_iterator<unsigned char>(meta));
        gLog.error() << "Failed to deserialize transaction. txn = " << txn.str()
                     << " - meta = " << meta.str() << " txn length = "
                     << std::to_string(blobs.transaction.size())
                     << " meta length = "
                     << std::to_string(blobs.metadata.size());
        throw e;
    }
}

std::pair<
    std::shared_ptr<ripple::STTx const>,
    std::shared_ptr<ripple::TxMeta const>>
deserializeTxPlusMeta(
    Backend::TransactionAndMetadata const& blobs,
    std::uint32_t seq)
{
    auto [tx, meta] = deserializeTxPlusMeta(blobs);

    std::shared_ptr<ripple::TxMeta> m =
        std::make_shared<ripple::TxMeta>(tx->getTransactionID(), seq, *meta);

    return {tx, m};
}

boost::json::object
toJson(ripple::STBase const& obj)
{
    boost::json::value value = boost::json::parse(
        obj.getJson(ripple::JsonOptions::none).toStyledString());

    return value.as_object();
}

std::pair<boost::json::object, boost::json::object>
toExpandedJson(Backend::TransactionAndMetadata const& blobs)
{
    auto [txn, meta] = deserializeTxPlusMeta(blobs, blobs.ledgerSequence);
    auto txnJson = toJson(*txn);
    auto metaJson = toJson(*meta);
    insertDeliveredAmount(metaJson, txn, meta, blobs.date);
    return {txnJson, metaJson};
}

bool
insertDeliveredAmount(
    boost::json::object& metaJson,
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta,
    uint32_t date)
{
    if (canHaveDeliveredAmount(txn, meta))
    {
        if (auto amt = getDeliveredAmount(txn, meta, meta->getLgrSeq(), date))
            metaJson["delivered_amount"] =
                toBoostJson(amt->getJson(ripple::JsonOptions::include_date));
        else
            metaJson["delivered_amount"] = "unavailable";
        return true;
    }
    return false;
}

boost::json::object
toJson(ripple::TxMeta const& meta)
{
    boost::json::value value = boost::json::parse(
        meta.getJson(ripple::JsonOptions::none).toStyledString());

    return value.as_object();
}

boost::json::value
toBoostJson(Json::Value const& value)
{
    boost::json::value boostValue = boost::json::parse(value.toStyledString());

    return boostValue;
}

boost::json::object
toJson(ripple::SLE const& sle)
{
    boost::json::value value = boost::json::parse(
        sle.getJson(ripple::JsonOptions::none).toStyledString());
    if (sle.getType() == ripple::ltACCOUNT_ROOT)
    {
        if (sle.isFieldPresent(ripple::sfEmailHash))
        {
            auto const& hash = sle.getFieldH128(ripple::sfEmailHash);
            std::string md5 = strHex(hash);
            boost::algorithm::to_lower(md5);
            value.as_object()["urlgravatar"] =
                str(boost::format("http://www.gravatar.com/avatar/%s") % md5);
        }
    }
    return value.as_object();
}

boost::json::object
toJson(ripple::LedgerInfo const& lgrInfo)
{
    boost::json::object header;
    header["ledger_sequence"] = lgrInfo.seq;
    header["ledger_hash"] = ripple::strHex(lgrInfo.hash);
    header["txns_hash"] = ripple::strHex(lgrInfo.txHash);
    header["state_hash"] = ripple::strHex(lgrInfo.accountHash);
    header["parent_hash"] = ripple::strHex(lgrInfo.parentHash);
    header["total_coins"] = ripple::to_string(lgrInfo.drops);
    header["close_flags"] = lgrInfo.closeFlags;

    // Always show fields that contribute to the ledger hash
    header["parent_close_time"] =
        lgrInfo.parentCloseTime.time_since_epoch().count();
    header["close_time"] = lgrInfo.closeTime.time_since_epoch().count();
    header["close_time_resolution"] = lgrInfo.closeTimeResolution.count();
    return header;
}

std::optional<std::uint32_t>
parseStringAsUInt(std::string const& value)
{
    std::optional<std::uint32_t> index = {};
    try
    {
        index = boost::lexical_cast<std::uint32_t>(value);
    }
    catch (boost::bad_lexical_cast const&)
    {
    }

    return index;
}

std::variant<Status, ripple::LedgerInfo>
ledgerInfoFromRequest(Context const& ctx)
{
    auto hashValue = ctx.params.contains("ledger_hash")
        ? ctx.params.at("ledger_hash")
        : nullptr;

    if (!hashValue.is_null())
    {
        if (!hashValue.is_string())
            return Status{
                RippledError::rpcINVALID_PARAMS, "ledgerHashNotString"};

        ripple::uint256 ledgerHash;
        if (!ledgerHash.parseHex(hashValue.as_string().c_str()))
            return Status{
                RippledError::rpcINVALID_PARAMS, "ledgerHashMalformed"};

        auto lgrInfo = ctx.backend->fetchLedgerByHash(ledgerHash, ctx.yield);

        if (!lgrInfo || lgrInfo->seq > ctx.range.maxSequence)
            return Status{RippledError::rpcLGR_NOT_FOUND, "ledgerNotFound"};

        return *lgrInfo;
    }

    auto indexValue = ctx.params.contains("ledger_index")
        ? ctx.params.at("ledger_index")
        : nullptr;

    std::optional<std::uint32_t> ledgerSequence = {};
    if (!indexValue.is_null())
    {
        if (indexValue.is_string())
        {
            boost::json::string const& stringIndex = indexValue.as_string();
            if (stringIndex == "validated")
                ledgerSequence = ctx.range.maxSequence;
            else
                ledgerSequence = parseStringAsUInt(stringIndex.c_str());
        }
        else if (indexValue.is_int64())
            ledgerSequence = indexValue.as_int64();
    }
    else
    {
        ledgerSequence = ctx.range.maxSequence;
    }

    if (!ledgerSequence)
        return Status{RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"};

    auto lgrInfo =
        ctx.backend->fetchLedgerBySequence(*ledgerSequence, ctx.yield);

    if (!lgrInfo || lgrInfo->seq > ctx.range.maxSequence)
        return Status{RippledError::rpcLGR_NOT_FOUND, "ledgerNotFound"};

    return *lgrInfo;
}

std::vector<unsigned char>
ledgerInfoToBlob(ripple::LedgerInfo const& info, bool includeHash)
{
    ripple::Serializer s;
    s.add32(info.seq);
    s.add64(info.drops.drops());
    s.addBitString(info.parentHash);
    s.addBitString(info.txHash);
    s.addBitString(info.accountHash);
    s.add32(info.parentCloseTime.time_since_epoch().count());
    s.add32(info.closeTime.time_since_epoch().count());
    s.add8(info.closeTimeResolution.count());
    s.add8(info.closeFlags);
    if (includeHash)
        s.addBitString(info.hash);
    return s.peekData();
}

std::uint64_t
getStartHint(ripple::SLE const& sle, ripple::AccountID const& accountID)
{
    if (sle.getType() == ripple::ltRIPPLE_STATE)
    {
        if (sle.getFieldAmount(ripple::sfLowLimit).getIssuer() == accountID)
            return sle.getFieldU64(ripple::sfLowNode);
        else if (
            sle.getFieldAmount(ripple::sfHighLimit).getIssuer() == accountID)
            return sle.getFieldU64(ripple::sfHighNode);
    }

    if (!sle.isFieldPresent(ripple::sfOwnerNode))
        return 0;

    return sle.getFieldU64(ripple::sfOwnerNode);
}

std::variant<Status, AccountCursor>
traverseOwnedNodes(
    BackendInterface const& backend,
    ripple::AccountID const& accountID,
    std::uint32_t sequence,
    std::uint32_t limit,
    std::optional<std::string> jsonCursor,
    boost::asio::yield_context& yield,
    std::function<void(ripple::SLE&&)> atOwnedNode)
{
    if (!backend.fetchLedgerObject(
            ripple::keylet::account(accountID).key, sequence, yield))
        return Status{RippledError::rpcACT_NOT_FOUND};

    auto maybeCursor = parseAccountCursor(backend, sequence, jsonCursor, yield);
    if (!maybeCursor)
        return Status(ripple::rpcINVALID_PARAMS, "Malformed cursor");

    auto [hexCursor, startHint] = *maybeCursor;

    return traverseOwnedNodes(
        backend,
        ripple::keylet::ownerDir(accountID),
        hexCursor,
        startHint,
        sequence,
        limit,
        jsonCursor,
        yield,
        atOwnedNode);
}

std::variant<Status, AccountCursor>
traverseOwnedNodes(
    BackendInterface const& backend,
    ripple::Keylet const& owner,
    ripple::uint256 const& hexMarker,
    std::uint32_t const startHint,
    std::uint32_t sequence,
    std::uint32_t limit,
    std::optional<std::string> jsonCursor,
    boost::asio::yield_context& yield,
    std::function<void(ripple::SLE&&)> atOwnedNode)
{
    auto cursor = AccountCursor({beast::zero, 0});

    auto const rootIndex = owner;
    auto currentIndex = rootIndex;

    std::vector<ripple::uint256> keys;
    // Only reserve 2048 nodes when fetching all owned ledger objects. If there
    // are more, then keys will allocate more memory, which is suboptimal, but
    // should only occur occasionally.
    keys.reserve(std::min(std::uint32_t{2048}, limit));

    auto start = std::chrono::system_clock::now();

    // If startAfter is not zero try jumping to that page using the hint
    if (hexMarker.isNonZero())
    {
        auto const hintIndex = ripple::keylet::page(rootIndex, startHint);
        auto hintDir =
            backend.fetchLedgerObject(hintIndex.key, sequence, yield);

        if (!hintDir)
            return Status(ripple::rpcINVALID_PARAMS, "Invalid marker");

        ripple::SerialIter it{hintDir->data(), hintDir->size()};
        ripple::SLE sle{it, hintIndex.key};

        if (auto const& indexes = sle.getFieldV256(ripple::sfIndexes);
            std::find(std::begin(indexes), std::end(indexes), hexMarker) ==
            std::end(indexes))
        {
            // result in empty dataset
            return AccountCursor({beast::zero, 0});
        }

        currentIndex = hintIndex;
        bool found = false;
        for (;;)
        {
            auto const ownerDir =
                backend.fetchLedgerObject(currentIndex.key, sequence, yield);

            if (!ownerDir)
                return Status(
                    ripple::rpcINVALID_PARAMS, "Owner directory not found");

            ripple::SerialIter it{ownerDir->data(), ownerDir->size()};
            ripple::SLE sle{it, currentIndex.key};

            for (auto const& key : sle.getFieldV256(ripple::sfIndexes))
            {
                if (!found)
                {
                    if (key == hexMarker)
                        found = true;
                }
                else
                {
                    keys.push_back(key);

                    if (--limit == 0)
                    {
                        break;
                    }
                }
            }

            auto const uNodeNext = sle.getFieldU64(ripple::sfIndexNext);

            if (limit == 0)
            {
                cursor = AccountCursor({keys.back(), uNodeNext});
                break;
            }

            if (uNodeNext == 0)
                break;

            currentIndex = ripple::keylet::page(rootIndex, uNodeNext);
        }
    }
    else
    {
        for (;;)
        {
            auto const ownerDir =
                backend.fetchLedgerObject(currentIndex.key, sequence, yield);

            if (!ownerDir)
                break;

            ripple::SerialIter it{ownerDir->data(), ownerDir->size()};
            ripple::SLE sle{it, currentIndex.key};

            for (auto const& key : sle.getFieldV256(ripple::sfIndexes))
            {
                keys.push_back(key);

                if (--limit == 0)
                    break;
            }

            auto const uNodeNext = sle.getFieldU64(ripple::sfIndexNext);

            if (limit == 0)
            {
                cursor = AccountCursor({keys.back(), uNodeNext});
                break;
            }

            if (uNodeNext == 0)
                break;

            currentIndex = ripple::keylet::page(rootIndex, uNodeNext);
        }
    }
    auto end = std::chrono::system_clock::now();

    gLog.debug() << "Time loading owned directories: "
                 << std::chrono::duration_cast<std::chrono::milliseconds>(
                        end - start)
                        .count()
                 << " milliseconds";

    start = std::chrono::system_clock::now();
    auto objects = backend.fetchLedgerObjects(keys, sequence, yield);
    end = std::chrono::system_clock::now();

    gLog.debug() << "Time loading owned entries: "
                 << std::chrono::duration_cast<std::chrono::milliseconds>(
                        end - start)
                        .count()
                 << " milliseconds";

    for (auto i = 0; i < objects.size(); ++i)
    {
        ripple::SerialIter it{objects[i].data(), objects[i].size()};
        atOwnedNode(ripple::SLE{it, keys[i]});
    }

    if (limit == 0)
        return cursor;

    return AccountCursor({beast::zero, 0});
}

std::shared_ptr<ripple::SLE const>
read(
    ripple::Keylet const& keylet,
    ripple::LedgerInfo const& lgrInfo,
    Context const& context)
{
    if (auto const blob = context.backend->fetchLedgerObject(
            keylet.key, lgrInfo.seq, context.yield);
        blob)
    {
        return std::make_shared<ripple::SLE const>(
            ripple::SerialIter{blob->data(), blob->size()}, keylet.key);
    }

    return nullptr;
}

std::optional<ripple::Seed>
parseRippleLibSeed(boost::json::value const& value)
{
    // ripple-lib encodes seed used to generate an Ed25519 wallet in a
    // non-standard way. While rippled never encode seeds that way, we
    // try to detect such keys to avoid user confusion.
    if (!value.is_string())
        return {};

    auto const result = ripple::decodeBase58Token(
        value.as_string().c_str(), ripple::TokenType::None);

    if (result.size() == 18 &&
        static_cast<std::uint8_t>(result[0]) == std::uint8_t(0xE1) &&
        static_cast<std::uint8_t>(result[1]) == std::uint8_t(0x4B))
        return ripple::Seed(ripple::makeSlice(result.substr(2)));

    return {};
}

std::variant<Status, std::pair<ripple::PublicKey, ripple::SecretKey>>
keypairFromRequst(boost::json::object const& request)
{
    bool const has_key_type = request.contains("key_type");

    // All of the secret types we allow, but only one at a time.
    // The array should be constexpr, but that makes Visual Studio unhappy.
    static std::string const secretTypes[]{
        "passphrase", "secret", "seed", "seed_hex"};

    // Identify which secret type is in use.
    std::string secretType = "";
    int count = 0;
    for (auto t : secretTypes)
    {
        if (request.contains(t))
        {
            ++count;
            secretType = t;
        }
    }

    if (count == 0)
        return Status{RippledError::rpcINVALID_PARAMS, "missing field secret"};

    if (count > 1)
    {
        return Status{
            RippledError::rpcINVALID_PARAMS,
            "Exactly one of the following must be specified: "
            " passphrase, secret, seed, or seed_hex"};
    }

    std::optional<ripple::KeyType> keyType;
    std::optional<ripple::Seed> seed;

    if (has_key_type)
    {
        if (!request.at("key_type").is_string())
            return Status{RippledError::rpcINVALID_PARAMS, "keyTypeNotString"};

        std::string key_type = request.at("key_type").as_string().c_str();
        keyType = ripple::keyTypeFromString(key_type);

        if (!keyType)
            return Status{
                RippledError::rpcINVALID_PARAMS, "invalidFieldKeyType"};

        if (secretType == "secret")
            return Status{
                RippledError::rpcINVALID_PARAMS,
                "The secret field is not allowed if key_type is used."};
    }

    // ripple-lib encodes seed used to generate an Ed25519 wallet in a
    // non-standard way. While we never encode seeds that way, we try
    // to detect such keys to avoid user confusion.
    if (secretType != "seed_hex")
    {
        seed = parseRippleLibSeed(request.at(secretType));

        if (seed)
        {
            // If the user passed in an Ed25519 seed but *explicitly*
            // requested another key type, return an error.
            if (keyType.value_or(ripple::KeyType::ed25519) !=
                ripple::KeyType::ed25519)
                return Status{
                    RippledError::rpcINVALID_PARAMS,
                    "Specified seed is for an Ed25519 wallet."};

            keyType = ripple::KeyType::ed25519;
        }
    }

    if (!keyType)
        keyType = ripple::KeyType::secp256k1;

    if (!seed)
    {
        if (has_key_type)
        {
            if (!request.at(secretType).is_string())
                return Status{
                    RippledError::rpcINVALID_PARAMS,
                    "secret value must be string"};

            std::string key = request.at(secretType).as_string().c_str();

            if (secretType == "seed")
                seed = ripple::parseBase58<ripple::Seed>(key);
            else if (secretType == "passphrase")
                seed = ripple::parseGenericSeed(key);
            else if (secretType == "seed_hex")
            {
                ripple::uint128 s;
                if (s.parseHex(key))
                    seed.emplace(ripple::Slice(s.data(), s.size()));
            }
        }
        else
        {
            if (!request.at("secret").is_string())
                return Status{
                    RippledError::rpcINVALID_PARAMS,
                    "field secret should be a string"};

            std::string secret = request.at("secret").as_string().c_str();
            seed = ripple::parseGenericSeed(secret);
        }
    }

    if (!seed)
        return Status{
            RippledError::rpcBAD_SEED,
            "Bad Seed: invalid field message secretType"};

    if (keyType != ripple::KeyType::secp256k1 &&
        keyType != ripple::KeyType::ed25519)
        return Status{
            RippledError::rpcINVALID_PARAMS,
            "keypairForSignature: invalid key type"};

    return generateKeyPair(*keyType, *seed);
}

std::vector<ripple::AccountID>
getAccountsFromTransaction(boost::json::object const& transaction)
{
    std::vector<ripple::AccountID> accounts = {};
    for (auto const& [key, value] : transaction)
    {
        if (value.is_object())
        {
            auto inObject = getAccountsFromTransaction(value.as_object());
            accounts.insert(accounts.end(), inObject.begin(), inObject.end());
        }
        else if (value.is_string())
        {
            auto account = accountFromStringStrict(value.as_string().c_str());
            if (account)
            {
                accounts.push_back(*account);
            }
        }
    }

    return accounts;
}

bool
isGlobalFrozen(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& issuer,
    boost::asio::yield_context& yield)
{
    if (ripple::isXRP(issuer))
        return false;

    auto key = ripple::keylet::account(issuer).key;
    auto blob = backend.fetchLedgerObject(key, sequence, yield);

    if (!blob)
        return false;

    ripple::SerialIter it{blob->data(), blob->size()};
    ripple::SLE sle{it, key};

    return sle.isFlag(ripple::lsfGlobalFreeze);
}

bool
isFrozen(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& account,
    ripple::Currency const& currency,
    ripple::AccountID const& issuer,
    boost::asio::yield_context& yield)
{
    if (ripple::isXRP(currency))
        return false;

    auto key = ripple::keylet::account(issuer).key;
    auto blob = backend.fetchLedgerObject(key, sequence, yield);

    if (!blob)
        return false;

    ripple::SerialIter it{blob->data(), blob->size()};
    ripple::SLE sle{it, key};

    if (sle.isFlag(ripple::lsfGlobalFreeze))
        return true;

    if (issuer != account)
    {
        key = ripple::keylet::line(account, issuer, currency).key;
        blob = backend.fetchLedgerObject(key, sequence, yield);

        if (!blob)
            return false;

        ripple::SerialIter issuerIt{blob->data(), blob->size()};
        ripple::SLE issuerLine{issuerIt, key};

        auto frozen =
            (issuer > account) ? ripple::lsfHighFreeze : ripple::lsfLowFreeze;

        if (issuerLine.isFlag(frozen))
            return true;
    }

    return false;
}

ripple::XRPAmount
xrpLiquid(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& id,
    boost::asio::yield_context& yield)
{
    auto key = ripple::keylet::account(id).key;
    auto blob = backend.fetchLedgerObject(key, sequence, yield);

    if (!blob)
        return beast::zero;

    ripple::SerialIter it{blob->data(), blob->size()};
    ripple::SLE sle{it, key};

    std::uint32_t const ownerCount = sle.getFieldU32(ripple::sfOwnerCount);

    auto const reserve =
        backend.fetchFees(sequence, yield)->accountReserve(ownerCount);

    auto const balance = sle.getFieldAmount(ripple::sfBalance);

    ripple::STAmount amount = balance - reserve;
    if (balance < reserve)
        amount.clear();

    return amount.xrp();
}

ripple::STAmount
accountFunds(
    BackendInterface const& backend,
    std::uint32_t const sequence,
    ripple::STAmount const& amount,
    ripple::AccountID const& id,
    boost::asio::yield_context& yield)
{
    if (!amount.native() && amount.getIssuer() == id)
    {
        return amount;
    }
    else
    {
        return accountHolds(
            backend,
            sequence,
            id,
            amount.getCurrency(),
            amount.getIssuer(),
            true,
            yield);
    }
}

ripple::STAmount
accountHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& account,
    ripple::Currency const& currency,
    ripple::AccountID const& issuer,
    bool const zeroIfFrozen,
    boost::asio::yield_context& yield)
{
    ripple::STAmount amount;
    if (ripple::isXRP(currency))
    {
        return {xrpLiquid(backend, sequence, account, yield)};
    }
    auto key = ripple::keylet::line(account, issuer, currency).key;

    auto const blob = backend.fetchLedgerObject(key, sequence, yield);

    if (!blob)
    {
        amount.clear({currency, issuer});
        return amount;
    }

    ripple::SerialIter it{blob->data(), blob->size()};
    ripple::SLE sle{it, key};

    if (zeroIfFrozen &&
        isFrozen(backend, sequence, account, currency, issuer, yield))
    {
        amount.clear(ripple::Issue(currency, issuer));
    }
    else
    {
        amount = sle.getFieldAmount(ripple::sfBalance);
        if (account > issuer)
        {
            // Put balance in account terms.
            amount.negate();
        }
        amount.setIssuer(issuer);
    }

    return amount;
}

ripple::Rate
transferRate(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& issuer,
    boost::asio::yield_context& yield)
{
    auto key = ripple::keylet::account(issuer).key;
    auto blob = backend.fetchLedgerObject(key, sequence, yield);

    if (blob)
    {
        ripple::SerialIter it{blob->data(), blob->size()};
        ripple::SLE sle{it, key};

        if (sle.isFieldPresent(ripple::sfTransferRate))
            return ripple::Rate{sle.getFieldU32(ripple::sfTransferRate)};
    }

    return ripple::parityRate;
}

boost::json::array
postProcessOrderBook(
    std::vector<Backend::LedgerObject> const& offers,
    ripple::Book const& book,
    ripple::AccountID const& takerID,
    Backend::BackendInterface const& backend,
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context& yield)
{
    boost::json::array jsonOffers;

    std::map<ripple::AccountID, ripple::STAmount> umBalance;

    bool globalFreeze =
        isGlobalFrozen(backend, ledgerSequence, book.out.account, yield) ||
        isGlobalFrozen(backend, ledgerSequence, book.out.account, yield);

    auto rate = transferRate(backend, ledgerSequence, book.out.account, yield);

    for (auto const& obj : offers)
    {
        try
        {
            ripple::SerialIter it{obj.blob.data(), obj.blob.size()};
            ripple::SLE offer{it, obj.key};
            ripple::uint256 bookDir =
                offer.getFieldH256(ripple::sfBookDirectory);

            auto const uOfferOwnerID = offer.getAccountID(ripple::sfAccount);
            auto const& saTakerGets = offer.getFieldAmount(ripple::sfTakerGets);
            auto const& saTakerPays = offer.getFieldAmount(ripple::sfTakerPays);
            ripple::STAmount saOwnerFunds;
            bool firstOwnerOffer = true;

            if (book.out.account == uOfferOwnerID)
            {
                // If an offer is selling issuer's own IOUs, it is fully
                // funded.
                saOwnerFunds = saTakerGets;
            }
            else if (globalFreeze)
            {
                // If either asset is globally frozen, consider all offers
                // that aren't ours to be totally unfunded
                saOwnerFunds.clear(book.out);
            }
            else
            {
                auto umBalanceEntry = umBalance.find(uOfferOwnerID);
                if (umBalanceEntry != umBalance.end())
                {
                    // Found in running balance table.

                    saOwnerFunds = umBalanceEntry->second;
                    firstOwnerOffer = false;
                }
                else
                {
                    bool zeroIfFrozen = true;
                    saOwnerFunds = accountHolds(
                        backend,
                        ledgerSequence,
                        uOfferOwnerID,
                        book.out.currency,
                        book.out.account,
                        zeroIfFrozen,
                        yield);

                    if (saOwnerFunds < beast::zero)
                        saOwnerFunds.clear();
                }
            }

            boost::json::object offerJson = toJson(offer);

            ripple::STAmount saTakerGetsFunded;
            ripple::STAmount saOwnerFundsLimit = saOwnerFunds;
            ripple::Rate offerRate = ripple::parityRate;
            ripple::STAmount dirRate =
                ripple::amountFromQuality(getQuality(bookDir));

            if (rate != ripple::parityRate
                // Have a tranfer fee.
                && takerID != book.out.account
                // Not taking offers of own IOUs.
                && book.out.account != uOfferOwnerID)
            // Offer owner not issuing ownfunds
            {
                // Need to charge a transfer fee to offer owner.
                offerRate = rate;
                saOwnerFundsLimit = ripple::divide(saOwnerFunds, offerRate);
            }

            if (saOwnerFundsLimit >= saTakerGets)
            {
                // Sufficient funds no shenanigans.
                saTakerGetsFunded = saTakerGets;
            }
            else
            {
                saTakerGetsFunded = saOwnerFundsLimit;
                offerJson["taker_gets_funded"] = toBoostJson(
                    saTakerGetsFunded.getJson(ripple::JsonOptions::none));
                offerJson["taker_pays_funded"] = toBoostJson(
                    std::min(
                        saTakerPays,
                        ripple::multiply(
                            saTakerGetsFunded, dirRate, saTakerPays.issue()))
                        .getJson(ripple::JsonOptions::none));
            }

            ripple::STAmount saOwnerPays = (ripple::parityRate == offerRate)
                ? saTakerGetsFunded
                : std::min(
                      saOwnerFunds,
                      ripple::multiply(saTakerGetsFunded, offerRate));

            umBalance[uOfferOwnerID] = saOwnerFunds - saOwnerPays;

            if (firstOwnerOffer)
                offerJson["owner_funds"] = saOwnerFunds.getText();

            offerJson["quality"] = dirRate.getText();

            jsonOffers.push_back(offerJson);
        }
        catch (std::exception const& e)
        {
            gLog.error() << "caught exception: " << e.what();
        }
    }
    return jsonOffers;
}

std::variant<Status, ripple::Book>
parseBook(boost::json::object const& request)
{
    if (!request.contains("taker_pays"))
        return Status{
            RippledError::rpcINVALID_PARAMS, "Missing field 'taker_pays'"};

    if (!request.contains("taker_gets"))
        return Status{
            RippledError::rpcINVALID_PARAMS, "Missing field 'taker_gets'"};

    if (!request.at("taker_pays").is_object())
        return Status{
            RippledError::rpcINVALID_PARAMS,
            "Field 'taker_pays' is not an object"};

    if (!request.at("taker_gets").is_object())
        return Status{
            RippledError::rpcINVALID_PARAMS,
            "Field 'taker_gets' is not an object"};

    auto taker_pays = request.at("taker_pays").as_object();
    if (!taker_pays.contains("currency"))
        return Status{RippledError::rpcSRC_CUR_MALFORMED};

    if (!taker_pays.at("currency").is_string())
        return Status{RippledError::rpcSRC_CUR_MALFORMED};

    auto taker_gets = request.at("taker_gets").as_object();
    if (!taker_gets.contains("currency"))
        return Status{RippledError::rpcDST_AMT_MALFORMED};

    if (!taker_gets.at("currency").is_string())
        return Status{
            RippledError::rpcDST_AMT_MALFORMED,
        };

    ripple::Currency pay_currency;
    if (!ripple::to_currency(
            pay_currency, taker_pays.at("currency").as_string().c_str()))
        return Status{RippledError::rpcSRC_CUR_MALFORMED};

    ripple::Currency get_currency;
    if (!ripple::to_currency(
            get_currency, taker_gets["currency"].as_string().c_str()))
        return Status{RippledError::rpcDST_AMT_MALFORMED};

    ripple::AccountID pay_issuer;
    if (taker_pays.contains("issuer"))
    {
        if (!taker_pays.at("issuer").is_string())
            return Status{
                RippledError::rpcINVALID_PARAMS, "takerPaysIssuerNotString"};

        if (!ripple::to_issuer(
                pay_issuer, taker_pays.at("issuer").as_string().c_str()))
            return Status{RippledError::rpcSRC_ISR_MALFORMED};

        if (pay_issuer == ripple::noAccount())
            return Status{RippledError::rpcSRC_ISR_MALFORMED};
    }
    else
    {
        pay_issuer = ripple::xrpAccount();
    }

    if (isXRP(pay_currency) && !isXRP(pay_issuer))
        return Status{
            RippledError::rpcSRC_ISR_MALFORMED,
            "Unneeded field 'taker_pays.issuer' for XRP currency "
            "specification."};

    if (!isXRP(pay_currency) && isXRP(pay_issuer))
        return Status{
            RippledError::rpcSRC_ISR_MALFORMED,
            "Invalid field 'taker_pays.issuer', expected non-XRP "
            "issuer."};

    if ((!isXRP(pay_currency)) && (!taker_pays.contains("issuer")))
        return Status{
            RippledError::rpcSRC_ISR_MALFORMED, "Missing non-XRP issuer."};

    ripple::AccountID get_issuer;

    if (taker_gets.contains("issuer"))
    {
        if (!taker_gets["issuer"].is_string())
            return Status{
                RippledError::rpcINVALID_PARAMS,
                "taker_gets.issuer should be string"};

        if (!ripple::to_issuer(
                get_issuer, taker_gets.at("issuer").as_string().c_str()))
            return Status{
                RippledError::rpcDST_ISR_MALFORMED,
                "Invalid field 'taker_gets.issuer', bad issuer."};

        if (get_issuer == ripple::noAccount())
            return Status{
                RippledError::rpcDST_ISR_MALFORMED,
                "Invalid field 'taker_gets.issuer', bad issuer account "
                "one."};
    }
    else
    {
        get_issuer = ripple::xrpAccount();
    }

    if (ripple::isXRP(get_currency) && !ripple::isXRP(get_issuer))
        return Status{
            RippledError::rpcDST_ISR_MALFORMED,
            "Unneeded field 'taker_gets.issuer' for XRP currency "
            "specification."};

    if (!ripple::isXRP(get_currency) && ripple::isXRP(get_issuer))
        return Status{
            RippledError::rpcDST_ISR_MALFORMED,
            "Invalid field 'taker_gets.issuer', expected non-XRP issuer."};

    if (pay_currency == get_currency && pay_issuer == get_issuer)
        return Status{RippledError::rpcBAD_MARKET, "badMarket"};

    return ripple::Book{{pay_currency, pay_issuer}, {get_currency, get_issuer}};
}

std::variant<Status, ripple::AccountID>
parseTaker(boost::json::value const& taker)
{
    std::optional<ripple::AccountID> takerID = {};
    if (!taker.is_string())
        return {Status{RippledError::rpcINVALID_PARAMS, "takerNotString"}};

    takerID = accountFromStringStrict(taker.as_string().c_str());

    if (!takerID)
        return Status{RippledError::rpcBAD_ISSUER, "invalidTakerAccount"};
    return *takerID;
}
bool
specifiesCurrentOrClosedLedger(boost::json::object const& request)
{
    if (request.contains("ledger_index"))
    {
        auto indexValue = request.at("ledger_index");
        if (indexValue.is_string())
        {
            std::string index = indexValue.as_string().c_str();
            return index == "current" || index == "closed";
        }
    }
    return false;
}

std::variant<ripple::uint256, Status>
getNFTID(boost::json::object const& request)
{
    if (!request.contains(JS(nft_id)))
        return Status{RippledError::rpcINVALID_PARAMS, "missingTokenID"};

    if (!request.at(JS(nft_id)).is_string())
        return Status{RippledError::rpcINVALID_PARAMS, "tokenIDNotString"};

    ripple::uint256 tokenid;
    if (!tokenid.parseHex(request.at(JS(nft_id)).as_string().c_str()))
        return Status{RippledError::rpcINVALID_PARAMS, "malformedTokenID"};

    return tokenid;
}

// TODO - this function is long and shouldn't be responsible for as much as it
// is. Split it out into some helper functions.
std::variant<Status, boost::json::object>
traverseTransactions(
    Context const& context,
    std::function<Backend::TransactionsAndCursor(
        std::shared_ptr<Backend::BackendInterface const> const& backend,
        std::uint32_t const,
        bool const,
        std::optional<Backend::TransactionsCursor> const&,
        boost::asio::yield_context& yield)> transactionFetcher)
{
    auto request = context.params;
    boost::json::object response = {};

    bool const binary = getBool(request, JS(binary), false);
    bool const forward = getBool(request, JS(forward), false);

    std::optional<Backend::TransactionsCursor> cursor;

    if (request.contains(JS(marker)))
    {
        if (!request.at(JS(marker)).is_object())
            return Status{RippledError::rpcINVALID_PARAMS, "invalidMarker"};
        auto const& obj = request.at(JS(marker)).as_object();

        std::optional<std::uint32_t> transactionIndex = {};
        if (obj.contains(JS(seq)))
        {
            if (!obj.at(JS(seq)).is_int64())
                return Status{
                    RippledError::rpcINVALID_PARAMS, "transactionIndexNotInt"};

            transactionIndex =
                boost::json::value_to<std::uint32_t>(obj.at(JS(seq)));
        }

        std::optional<std::uint32_t> ledgerIndex = {};
        if (obj.contains(JS(ledger)))
        {
            if (!obj.at(JS(ledger)).is_int64())
                return Status{
                    RippledError::rpcINVALID_PARAMS, "ledgerIndexNotInt"};

            ledgerIndex =
                boost::json::value_to<std::uint32_t>(obj.at(JS(ledger)));
        }

        if (!transactionIndex || !ledgerIndex)
            return Status{
                RippledError::rpcINVALID_PARAMS, "missingLedgerOrSeq"};

        cursor = {*ledgerIndex, *transactionIndex};
    }

    auto minIndex = context.range.minSequence;
    auto maxIndex = context.range.maxSequence;
    std::optional<int64_t> min;
    std::optional<int64_t> max;

    if (request.contains(JS(ledger_index_min)))
    {
        if (!request.at(JS(ledger_index_min)).is_int64())
        {
            return Status{
                RippledError::rpcINVALID_PARAMS, "ledgerSeqMinNotNumber"};
        }

        min = request.at(JS(ledger_index_min)).as_int64();

        if (*min != -1)
        {
            if (context.range.maxSequence < *min ||
                context.range.minSequence > *min)
                return Status{
                    RippledError::rpcLGR_IDX_MALFORMED,
                    "ledgerSeqMinOutOfRange"};
            else
                minIndex = static_cast<uint32_t>(*min);
        }

        if (forward && !cursor)
            cursor = {minIndex, 0};
    }

    if (request.contains(JS(ledger_index_max)))
    {
        if (!request.at(JS(ledger_index_max)).is_int64())
        {
            return Status{
                RippledError::rpcINVALID_PARAMS, "ledgerSeqMaxNotNumber"};
        }

        max = request.at(JS(ledger_index_max)).as_int64();

        if (*max != -1)
        {
            if (context.range.maxSequence < *max ||
                context.range.minSequence > *max)
                return Status{
                    RippledError::rpcLGR_IDX_MALFORMED,
                    "ledgerSeqMaxOutOfRange"};
            else
                maxIndex = static_cast<uint32_t>(*max);
        }

        if (minIndex > maxIndex)
            return Status{RippledError::rpcINVALID_PARAMS, "invalidIndex"};

        if (!forward && !cursor)
            cursor = {maxIndex, INT32_MAX};
    }

    if (max && min && *max < *min)
    {
        return Status{RippledError::rpcLGR_IDXS_INVALID, "lgrIdxsInvalid"};
    }

    if (request.contains(JS(ledger_index)) || request.contains(JS(ledger_hash)))
    {
        if (request.contains(JS(ledger_index_max)) ||
            request.contains(JS(ledger_index_min)))
            return Status{
                RippledError::rpcINVALID_PARAMS,
                "containsLedgerSpecifierAndRange"};

        auto v = ledgerInfoFromRequest(context);
        if (auto status = std::get_if<Status>(&v); status)
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
    auto [blobs, retCursor] = transactionFetcher(
        context.backend, limit, forward, cursor, context.yield);
    auto serializationStart = std::chrono::system_clock::now();

    if (retCursor)
    {
        boost::json::object cursorJson;
        cursorJson[JS(ledger)] = retCursor->ledgerSequence;
        cursorJson[JS(seq)] = retCursor->transactionIndex;
        response[JS(marker)] = cursorJson;
    }

    for (auto const& txnPlusMeta : blobs)
    {
        if ((txnPlusMeta.ledgerSequence < minIndex && !forward) ||
            (txnPlusMeta.ledgerSequence > maxIndex && forward))
        {
            response.erase(JS(marker));
            break;
        }
        else if (txnPlusMeta.ledgerSequence > maxIndex && !forward)
        {
            gLog.debug() << "Skipping over transactions from incomplete ledger";
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
    }

    response[JS(ledger_index_min)] = minIndex;
    response[JS(ledger_index_max)] = maxIndex;
    response[JS(transactions)] = txns;

    gLog.info() << "serialization took "
                << std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now() - serializationStart)
                       .count()
                << " milliseconds";

    return response;
}

}  // namespace RPC
