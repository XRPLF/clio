#include <handlers/RPCHelpers.h>
#include <handlers/Status.h>
#include <backend/BackendInterface.h>

std::optional<ripple::AccountID>
accountFromStringStrict(std::string const& account)
{
    auto blob = ripple::strUnHex(account);

    boost::optional<ripple::PublicKey> publicKey = {};
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
toJson(ripple::TxMeta const& meta)
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
toBoostJson(Json::Value const& value)
{
    boost::json::value boostValue = 
        boost::json::parse(value.toStyledString());
    
    return boostValue;
}

boost::json::object
toJson(ripple::TxMeta const& meta)
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

boost::json::object
toJson(ripple::SLE const& sle)
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

std::variant<RPC::Status, ripple::LedgerInfo>
ledgerInfoFromRequest(RPC::Context const& ctx)
{
    auto indexValue = ctx.params.at("ledger_index");
    auto hashValue = ctx.params.at("ledger_hash");

    std::optional<ripple::LedgerInfo> lgrInfo;
    if (!hashValue.is_null())
    {
        if (!hashValue.is_string())
            return RPC::Status{RPC::Error::rpcINVALID_PARAMS, "ledgerHashNotString"};

        ripple::uint256 ledgerHash;
        if (!ledgerHash.parseHex(hashValue.as_string().c_str()))
            return RPC::Status{RPC::Error::rpcINVALID_PARAMS, "ledgerHashMalformed"};

        lgrInfo = ctx.backend->fetchLedgerByHash(ledgerHash);

    }
    else if (!indexValue.is_null())
    {
        std::uint32_t ledgerSequence;
        if (indexValue.is_string() && indexValue.as_string() == "validated")
            ledgerSequence = ctx.range.maxSequence;
        else if (!indexValue.is_string() && indexValue.is_int64())
            ledgerSequence = indexValue.as_int64();
        else
            return RPC::Status{RPC::Error::rpcINVALID_PARAMS, "ledgerIndexMalformed"};

        lgrInfo =
            ctx.backend->fetchLedgerBySequence(ledgerSequence);
    }
    else
    {
        lgrInfo = 
            ctx.backend->fetchLedgerBySequence(ctx.range.maxSequence);
    }

    if (!lgrInfo)
        return RPC::Status{RPC::Error::rpcLGR_NOT_FOUND, "ledgerNotFound"};

    return *lgrInfo;
}

std::vector<unsigned char>
ledgerInfoToBlob(ripple::LedgerInfo const& info)
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
    // s.addBitString(info.hash);
    return s.peekData();
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
        auto ownedNode = backend.fetchLedgerObject(currentIndex.key, sequence);

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
            nextCursor = keys[i + 1];
            break;
        }
    }

    return nextCursor;
}

boost::optional<ripple::Seed>
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

std::variant<RPC::Status, std::pair<ripple::PublicKey, ripple::SecretKey>>
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
        return RPC::Status{RPC::Error::rpcINVALID_PARAMS, "missing field secret"};

    if (count > 1)
    {
        return RPC::Status{RPC::Error::rpcINVALID_PARAMS, 
                "Exactly one of the following must be specified: "
                " passphrase, secret, seed, or seed_hex"};
    }

    boost::optional<ripple::KeyType> keyType;
    boost::optional<ripple::Seed> seed;

    if (has_key_type)
    {
        if (!request.at("key_type").is_string())
            return RPC::Status{RPC::Error::rpcINVALID_PARAMS, "keyTypeNotString"};

        std::string key_type = request.at("key_type").as_string().c_str();
        keyType = ripple::keyTypeFromString(key_type);

        if (!keyType)
            return RPC::Status{RPC::Error::rpcINVALID_PARAMS, "invalidFieldKeyType"};

        if (secretType == "secret")
            return RPC::Status{RPC::Error::rpcINVALID_PARAMS,
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
            if (keyType.value_or(ripple::KeyType::ed25519) != ripple::KeyType::ed25519)
                return RPC::Status{RPC::Error::rpcINVALID_PARAMS,
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
                return RPC::Status{RPC::Error::rpcINVALID_PARAMS,
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
                return RPC::Status{RPC::Error::rpcINVALID_PARAMS,
                                  "field secret should be a string"};

            std::string secret = request.at("secret").as_string().c_str();
            seed = ripple::parseGenericSeed(secret);
        }
    }

    if (!seed)
        return RPC::Status{RPC::Error::rpcBAD_SEED,
                "Bad Seed: invalid field message secretType"};

    if (keyType != ripple::KeyType::secp256k1
     && keyType != ripple::KeyType::ed25519)
        return RPC::Status{RPC::Error::rpcINVALID_PARAMS, 
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

std::vector<unsigned char>
ledgerInfoToBlob(ripple::LedgerInfo const& info)
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
    s.addBitString(info.hash);
    return s.peekData();
}
