#include <backend/BackendInterface.h>
#include <rpc/RPCHelpers.h>
namespace RPC {

std::optional<ripple::STAmount>
getDeliveredAmount(
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta,
    uint32_t ledgerSequence)
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
        if (ledgerSequence >= 4594095)
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
        BOOST_LOG_TRIVIAL(error)
            << __func__
            << " Failed to deserialize transaction. txn = " << txn.str()
            << " - meta = " << meta.str()
            << " txn length = " << std::to_string(blobs.transaction.size())
            << " meta length = " << std::to_string(blobs.metadata.size());
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
    insertDeliveredAmount(metaJson, txn, meta);
    return {txnJson, metaJson};
}

bool
insertDeliveredAmount(
    boost::json::object& metaJson,
    std::shared_ptr<ripple::STTx const> const& txn,
    std::shared_ptr<ripple::TxMeta const> const& meta)
{
    if (canHaveDeliveredAmount(txn, meta))
    {
        if (auto amt = getDeliveredAmount(txn, meta, meta->getLgrSeq()))
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

std::variant<Status, ripple::LedgerInfo>
ledgerInfoFromRequest(Context const& ctx)
{
    auto indexValue = ctx.params.contains("ledger_index")
        ? ctx.params.at("ledger_index")
        : nullptr;

    auto hashValue = ctx.params.contains("ledger_hash")
        ? ctx.params.at("ledger_hash")
        : nullptr;

    std::optional<ripple::LedgerInfo> lgrInfo;
    if (!hashValue.is_null())
    {
        if (!hashValue.is_string())
            return Status{Error::rpcINVALID_PARAMS, "ledgerHashNotString"};

        ripple::uint256 ledgerHash;
        if (!ledgerHash.parseHex(hashValue.as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "ledgerHashMalformed"};

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
            return Status{Error::rpcINVALID_PARAMS, "ledgerIndexMalformed"};

        lgrInfo = ctx.backend->fetchLedgerBySequence(ledgerSequence);
    }
    else
    {
        lgrInfo = ctx.backend->fetchLedgerBySequence(ctx.range.maxSequence);
    }

    if (!lgrInfo)
        return Status{Error::rpcLGR_NOT_FOUND, "ledgerNotFound"};

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
        return Status{Error::rpcINVALID_PARAMS, "missing field secret"};

    if (count > 1)
    {
        return Status{
            Error::rpcINVALID_PARAMS,
            "Exactly one of the following must be specified: "
            " passphrase, secret, seed, or seed_hex"};
    }

    std::optional<ripple::KeyType> keyType;
    std::optional<ripple::Seed> seed;

    if (has_key_type)
    {
        if (!request.at("key_type").is_string())
            return Status{Error::rpcINVALID_PARAMS, "keyTypeNotString"};

        std::string key_type = request.at("key_type").as_string().c_str();
        keyType = ripple::keyTypeFromString(key_type);

        if (!keyType)
            return Status{Error::rpcINVALID_PARAMS, "invalidFieldKeyType"};

        if (secretType == "secret")
            return Status{
                Error::rpcINVALID_PARAMS,
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
                    Error::rpcINVALID_PARAMS,
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
                    Error::rpcINVALID_PARAMS, "secret value must be string"};

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
                    Error::rpcINVALID_PARAMS,
                    "field secret should be a string"};

            std::string secret = request.at("secret").as_string().c_str();
            seed = ripple::parseGenericSeed(secret);
        }
    }

    if (!seed)
        return Status{
            Error::rpcBAD_SEED, "Bad Seed: invalid field message secretType"};

    if (keyType != ripple::KeyType::secp256k1 &&
        keyType != ripple::KeyType::ed25519)
        return Status{
            Error::rpcINVALID_PARAMS, "keypairForSignature: invalid key type"};

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
    ripple::AccountID const& issuer)
{
    if (ripple::isXRP(issuer))
        return false;

    auto key = ripple::keylet::account(issuer).key;
    auto blob = backend.fetchLedgerObject(key, sequence);

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
    ripple::AccountID const& issuer)
{
    if (ripple::isXRP(currency))
        return false;

    auto key = ripple::keylet::account(issuer).key;
    auto blob = backend.fetchLedgerObject(key, sequence);

    if (!blob)
        return false;

    ripple::SerialIter it{blob->data(), blob->size()};
    ripple::SLE sle{it, key};

    if (sle.isFlag(ripple::lsfGlobalFreeze))
        return true;

    if (issuer != account)
    {
        key = ripple::keylet::line(account, issuer, currency).key;
        blob = backend.fetchLedgerObject(key, sequence);

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
    ripple::AccountID const& id)
{
    auto key = ripple::keylet::account(id).key;
    auto blob = backend.fetchLedgerObject(key, sequence);

    if (!blob)
        return beast::zero;

    ripple::SerialIter it{blob->data(), blob->size()};
    ripple::SLE sle{it, key};

    std::uint32_t const ownerCount = sle.getFieldU32(ripple::sfOwnerCount);

    auto const reserve =
        backend.fetchFees(sequence)->accountReserve(ownerCount);

    auto const balance = sle.getFieldAmount(ripple::sfBalance);

    ripple::STAmount amount = balance - reserve;
    if (balance < reserve)
        amount.clear();

    return amount.xrp();
}

ripple::STAmount
accountFunds(
    BackendInterface const& backend,
    uint32_t sequence,
    ripple::STAmount const& amount,
    ripple::AccountID const& id)
{
    if (!amount.native() && amount.getIssuer() == id)
    {
        return amount;
    }
    else
    {
        return accountHolds(
            backend, sequence, id, amount.getCurrency(), amount.getIssuer());
    }
}

ripple::STAmount
accountHolds(
    BackendInterface const& backend,
    std::uint32_t sequence,
    ripple::AccountID const& account,
    ripple::Currency const& currency,
    ripple::AccountID const& issuer,
    bool zeroIfFrozen)
{
    ripple::STAmount amount;
    if (ripple::isXRP(currency))
    {
        return {xrpLiquid(backend, sequence, account)};
    }
    auto key = ripple::keylet::line(account, issuer, currency).key;

    auto const blob = backend.fetchLedgerObject(key, sequence);

    if (!blob)
    {
        amount.clear({currency, issuer});
        return amount;
    }

    ripple::SerialIter it{blob->data(), blob->size()};
    ripple::SLE sle{it, key};

    if (zeroIfFrozen && isFrozen(backend, sequence, account, currency, issuer))
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
    ripple::AccountID const& issuer)
{
    auto key = ripple::keylet::account(issuer).key;
    auto blob = backend.fetchLedgerObject(key, sequence);

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
    uint32_t ledgerSequence)
{
    boost::json::array jsonOffers;

    std::map<ripple::AccountID, ripple::STAmount> umBalance;

    bool globalFreeze =
        isGlobalFrozen(backend, ledgerSequence, book.out.account) ||
        isGlobalFrozen(backend, ledgerSequence, book.out.account);

    auto rate = transferRate(backend, ledgerSequence, book.out.account);

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
                        zeroIfFrozen);

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
                offerJson["taker_gets_funded"] = saTakerGetsFunded.getText();
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
            BOOST_LOG_TRIVIAL(error) << "caught exception: " << e.what();
        }
    }
    return jsonOffers;
}

std::variant<Status, ripple::Book>
parseBook(boost::json::object const& request)
{
    if (!request.contains("taker_pays"))
        return Status{Error::rpcINVALID_PARAMS, "missingTakerPays"};

    if (!request.contains("taker_gets"))
        return Status{Error::rpcINVALID_PARAMS, "missingTakerGets"};

    if (!request.at("taker_pays").is_object())
        return Status{Error::rpcINVALID_PARAMS, "takerPaysNotObject"};

    if (!request.at("taker_gets").is_object())
        return Status{Error::rpcINVALID_PARAMS, "takerGetsNotObject"};

    auto taker_pays = request.at("taker_pays").as_object();
    if (!taker_pays.contains("currency"))
        return Status{Error::rpcINVALID_PARAMS, "missingTakerPaysCurrency"};

    if (!taker_pays.at("currency").is_string())
        return Status{Error::rpcINVALID_PARAMS, "takerPaysCurrencyNotString"};

    auto taker_gets = request.at("taker_gets").as_object();
    if (!taker_gets.contains("currency"))
        return Status{Error::rpcINVALID_PARAMS, "missingTakerGetsCurrency"};

    if (!taker_gets.at("currency").is_string())
        return Status{Error::rpcINVALID_PARAMS, "takerGetsCurrencyNotString"};

    ripple::Currency pay_currency;
    if (!ripple::to_currency(
            pay_currency, taker_pays.at("currency").as_string().c_str()))
        return Status{Error::rpcINVALID_PARAMS, "badTakerPaysCurrency"};

    ripple::Currency get_currency;
    if (!ripple::to_currency(
            get_currency, taker_gets["currency"].as_string().c_str()))
        return Status{Error::rpcINVALID_PARAMS, "badTakerGetsCurrency"};

    ripple::AccountID pay_issuer;
    if (taker_pays.contains("issuer"))
    {
        if (!taker_pays.at("issuer").is_string())
            return Status{Error::rpcINVALID_PARAMS, "takerPaysIssuerNotString"};

        if (!ripple::to_issuer(
                pay_issuer, taker_pays.at("issuer").as_string().c_str()))
            return Status{Error::rpcINVALID_PARAMS, "badTakerPaysIssuer"};

        if (pay_issuer == ripple::noAccount())
            return Status{
                Error::rpcINVALID_PARAMS, "badTakerPaysIssuerAccountOne"};
    }
    else
    {
        pay_issuer = ripple::xrpAccount();
    }

    if (isXRP(pay_currency) && !isXRP(pay_issuer))
        return Status{
            Error::rpcINVALID_PARAMS,
            "Unneeded field 'taker_pays.issuer' for XRP currency "
            "specification."};

    if (!isXRP(pay_currency) && isXRP(pay_issuer))
        return Status{
            Error::rpcINVALID_PARAMS,
            "Invalid field 'taker_pays.issuer', expected non-XRP "
            "issuer."};

    ripple::AccountID get_issuer;

    if (taker_gets.contains("issuer"))
    {
        if (!taker_gets["issuer"].is_string())
            return Status{
                Error::rpcINVALID_PARAMS, "taker_gets.issuer should be string"};

        if (!ripple::to_issuer(
                get_issuer, taker_gets.at("issuer").as_string().c_str()))
            return Status{
                Error::rpcINVALID_PARAMS,
                "Invalid field 'taker_gets.issuer', bad issuer."};

        if (get_issuer == ripple::noAccount())
            return Status{
                Error::rpcINVALID_PARAMS,
                "Invalid field 'taker_gets.issuer', bad issuer account "
                "one."};
    }
    else
    {
        get_issuer = ripple::xrpAccount();
    }

    if (ripple::isXRP(get_currency) && !ripple::isXRP(get_issuer))
        return Status{
            Error::rpcINVALID_PARAMS,
            "Unneeded field 'taker_gets.issuer' for XRP currency "
            "specification."};

    if (!ripple::isXRP(get_currency) && ripple::isXRP(get_issuer))
        return Status{
            Error::rpcINVALID_PARAMS,
            "Invalid field 'taker_gets.issuer', expected non-XRP issuer."};

    if (pay_currency == get_currency && pay_issuer == get_issuer)
        return Status{Error::rpcINVALID_PARAMS, "badMarket"};

    return ripple::Book{{pay_currency, pay_issuer}, {get_currency, get_issuer}};
}
std::variant<Status, ripple::AccountID>
parseTaker(boost::json::value const& taker)
{
    std::optional<ripple::AccountID> takerID = {};
    if (!taker.is_string())
        return {Status{Error::rpcINVALID_PARAMS, "takerNotString"}};

    takerID = accountFromStringStrict(taker.as_string().c_str());

    if (!takerID)
        return Status{Error::rpcINVALID_PARAMS, "invalidTakerAccount"};
    return *takerID;
}

}  // namespace RPC
