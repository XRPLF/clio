#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <backend/BackendInterface.h>
namespace Backend {
bool
BackendInterface::finishWrites(uint32_t ledgerSequence)
{
    indexer_.finish(ledgerSequence, *this);
    auto commitRes = doFinishWrites();
    if (commitRes)
    {
        if (isFirst_)
            indexer_.doKeysRepairAsync(*this, ledgerSequence);
        if (indexer_.isKeyFlagLedger(ledgerSequence))
            indexer_.writeKeyFlagLedgerAsync(ledgerSequence, *this);
        isFirst_ = false;
        updateRange(ledgerSequence);
    }
    else
    {
        // if commitRes is false, we are relinquishing control of ETL. We
        // reset isFirst_ to true so that way if we later regain control of
        // ETL, we trigger the index repair
        isFirst_ = true;
    }
    return commitRes;
}
bool
BackendInterface::isLedgerIndexed(std::uint32_t ledgerSequence) const
{
    auto keyIndex = getKeyIndexOfSeq(ledgerSequence);
    if (keyIndex)
    {
        auto page = doFetchLedgerPage({}, ledgerSequence, 1);
        return !page.warning.has_value();
    }
    return false;
}
void
BackendInterface::writeLedgerObject(
    std::string&& key,
    uint32_t seq,
    std::string&& blob) const
{
    assert(key.size() == sizeof(ripple::uint256));
    ripple::uint256 key256 = ripple::uint256::fromVoid(key.data());
    indexer_.addKey(std::move(key256));
    doWriteLedgerObject(std::move(key), seq, std::move(blob));
}
std::optional<LedgerRange>
BackendInterface::hardFetchLedgerRangeNoThrow() const
{
    BOOST_LOG_TRIVIAL(debug) << __func__;
    while (true)
    {
        try
        {
            return hardFetchLedgerRange();
        }
        catch (DatabaseTimeout& t)
        {
            ;
        }
    }
}
std::optional<KeyIndex>
BackendInterface::getKeyIndexOfSeq(uint32_t seq) const
{
    if (indexer_.isKeyFlagLedger(seq))
        return KeyIndex{seq};
    auto rng = fetchLedgerRange();
    if (!rng)
        return {};
    if (rng->minSequence == seq)
        return KeyIndex{seq};
    return indexer_.getKeyIndexOfSeq(seq);
}
BookOffersPage
BackendInterface::fetchBookOffers(
    ripple::uint256 const& book,
    uint32_t ledgerSequence,
    std::uint32_t limit,
    std::optional<ripple::uint256> const& cursor) const
{
    // TODO try to speed this up. This can take a few seconds. The goal is to
    // get it down to a few hundred milliseconds.
    BookOffersPage page;
    const ripple::uint256 bookEnd = ripple::getQualityNext(book);
    ripple::uint256 uTipIndex = book;
    bool done = false;
    std::vector<ripple::uint256> keys;
    auto getMillis = [](auto diff) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(diff)
            .count();
    };
    auto begin = std::chrono::system_clock::now();
    uint32_t numSucc = 0;
    uint32_t numPages = 0;
    long succMillis = 0;
    long pageMillis = 0;
    while (keys.size() < limit)
    {
        auto mid1 = std::chrono::system_clock::now();
        auto offerDir = fetchSuccessor(uTipIndex, ledgerSequence);
        auto mid2 = std::chrono::system_clock::now();
        numSucc++;
        succMillis += getMillis(mid2 - mid1);
        if (!offerDir || offerDir->key > bookEnd)
        {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " - offerDir.has_value() "
                                     << offerDir.has_value() << " breaking";
            break;
        }
        while (keys.size() < limit)
        {
            ++numPages;
            uTipIndex = offerDir->key;
            ripple::STLedgerEntry sle{
                ripple::SerialIter{
                    offerDir->blob.data(), offerDir->blob.size()},
                offerDir->key};
            auto indexes = sle.getFieldV256(ripple::sfIndexes);
            keys.insert(keys.end(), indexes.begin(), indexes.end());
            auto next = sle.getFieldU64(ripple::sfIndexNext);
            if (!next)
            {
                BOOST_LOG_TRIVIAL(debug)
                    << __func__ << " next is empty. breaking";
                break;
            }
            auto nextKey = ripple::keylet::page(uTipIndex, next);
            auto nextDir = fetchLedgerObject(nextKey.key, ledgerSequence);
            assert(nextDir);
            offerDir->blob = *nextDir;
            offerDir->key = nextKey.key;
        }
        auto mid3 = std::chrono::system_clock::now();
        pageMillis += getMillis(mid3 - mid2);
    }
    auto mid = std::chrono::system_clock::now();
    auto objs = fetchLedgerObjects(keys, ledgerSequence);
    for (size_t i = 0; i < keys.size() && i < limit; ++i)
    {
        BOOST_LOG_TRIVIAL(trace)
            << __func__ << " key = " << ripple::strHex(keys[i])
            << " blob = " << ripple::strHex(objs[i]);
        assert(objs[i].size());
        page.offers.push_back({keys[i], objs[i]});
    }
    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " "
        << "Fetching " << std::to_string(keys.size()) << " offers took "
        << std::to_string(getMillis(mid - begin))
        << " milliseconds. Fetching next dir took "
        << std::to_string(succMillis) << " milliseonds. Fetched next dir "
        << std::to_string(numSucc) << " times"
        << " Fetching next page of dir took " << std::to_string(pageMillis)
        << " milliseconds"
        << ". num pages = " << std::to_string(numPages)
        << ". Fetching all objects took "
        << std::to_string(getMillis(end - mid))
        << " milliseconds. total time = "
        << std::to_string(getMillis(end - begin)) << " milliseconds";

    return page;
}

std::optional<LedgerObject>
BackendInterface::fetchSuccessor(ripple::uint256 key, uint32_t ledgerSequence)
    const
{
    auto start = std::chrono::system_clock::now();
    auto page = fetchLedgerPage({++key}, ledgerSequence, 1, 512);
    auto end = std::chrono::system_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                  .count();
    BOOST_LOG_TRIVIAL(debug)
        << __func__ << " took " << std::to_string(ms) << " milliseconds";
    if (page.objects.size())
        return page.objects[0];
    return {};
}
LedgerPage
BackendInterface::fetchLedgerPage(
    std::optional<ripple::uint256> const& cursor,
    std::uint32_t ledgerSequence,
    std::uint32_t limit,
    std::uint32_t limitHint) const
{
    assert(limit != 0);
    bool incomplete = !isLedgerIndexed(ledgerSequence);
    BOOST_LOG_TRIVIAL(debug) << __func__ << " incomplete = " << incomplete;
    // really low limits almost always miss
    uint32_t adjustedLimit = std::max(limitHint, std::max(limit, (uint32_t)4));
    LedgerPage page;
    page.cursor = cursor;
    long totalTime = 0;
    do
    {
        adjustedLimit = adjustedLimit >= 8192 ? 8192 : adjustedLimit * 2;
        auto start = std::chrono::system_clock::now();
        auto partial =
            doFetchLedgerPage(page.cursor, ledgerSequence, adjustedLimit);
        auto end = std::chrono::system_clock::now();
        std::string pageCursorStr =
            page.cursor ? ripple::strHex(*page.cursor) : "";
        std::string partialCursorStr =
            partial.cursor ? ripple::strHex(*partial.cursor) : "";
        auto thisTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count();
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " " << std::to_string(ledgerSequence) << " "
            << std::to_string(adjustedLimit) << " " << pageCursorStr << " - "
            << partialCursorStr << " - time = " << std::to_string(thisTime);
        totalTime += thisTime;
        page.objects.insert(
            page.objects.end(), partial.objects.begin(), partial.objects.end());
        page.cursor = partial.cursor;
    } while (page.objects.size() < limit && page.cursor && totalTime < 5000);
    if (incomplete)
    {
        auto rng = fetchLedgerRange();
        if (!rng)
            return page;
        if (rng->minSequence == ledgerSequence)
        {
            BOOST_LOG_TRIVIAL(fatal)
                << __func__
                << " Database is populated but first flag ledger is "
                   "incomplete. This should never happen";
            assert(false);
            throw std::runtime_error("Missing base flag ledger");
        }
        uint32_t lowerSequence = (ledgerSequence - 1) >> indexer_.getKeyShift()
                << indexer_.getKeyShift();
        if (lowerSequence < rng->minSequence)
            lowerSequence = rng->minSequence;
        BOOST_LOG_TRIVIAL(debug)
            << __func__
            << " recursing. ledgerSequence = " << std::to_string(ledgerSequence)
            << " , lowerSequence = " << std::to_string(lowerSequence);
        auto lowerPage = fetchLedgerPage(cursor, lowerSequence, limit);
        std::vector<ripple::uint256> keys;
        std::transform(
            std::move_iterator(lowerPage.objects.begin()),
            std::move_iterator(lowerPage.objects.end()),
            std::back_inserter(keys),
            [](auto&& elt) { return std::move(elt.key); });
        size_t upperPageSize = page.objects.size();
        auto objs = fetchLedgerObjects(keys, ledgerSequence);
        for (size_t i = 0; i < keys.size(); ++i)
        {
            auto& obj = objs[i];
            auto& key = keys[i];
            if (obj.size())
                page.objects.push_back({std::move(key), std::move(obj)});
        }
        std::sort(page.objects.begin(), page.objects.end(), [](auto a, auto b) {
            return a.key < b.key;
        });
        if (page.objects.size() > limit)
            page.objects.resize(limit);
        if (page.objects.size() && page.objects.size() >= limit)
            page.cursor = page.objects.back().key;
    }
    return page;
}

void
BackendInterface::checkFlagLedgers() const
{
    auto rng = hardFetchLedgerRangeNoThrow();
    if (rng)
    {
        bool prevComplete = true;
        uint32_t cur = rng->minSequence;
        size_t numIncomplete = 0;
        while (cur <= rng->maxSequence + 1)
        {
            auto keyIndex = getKeyIndexOfSeq(cur);
            assert(keyIndex.has_value());
            cur = keyIndex->keyIndex;

            if (!isLedgerIndexed(cur))
            {
                BOOST_LOG_TRIVIAL(warning)
                    << __func__ << " - flag ledger "
                    << std::to_string(keyIndex->keyIndex) << " is incomplete";
                ++numIncomplete;
                prevComplete = false;
            }
            else
            {
                if (!prevComplete)
                {
                    BOOST_LOG_TRIVIAL(fatal)
                        << __func__ << " - flag ledger "
                        << std::to_string(keyIndex->keyIndex)
                        << " is incomplete but the next is complete. This "
                           "should never happen";
                    assert(false);
                    throw std::runtime_error("missing prev flag ledger");
                }
                prevComplete = true;
                BOOST_LOG_TRIVIAL(info)
                    << __func__ << " - flag ledger "
                    << std::to_string(keyIndex->keyIndex) << " is complete";
            }
            cur = cur + 1;
        }
        if (numIncomplete > 1)
        {
            BOOST_LOG_TRIVIAL(warning)
                << __func__ << " " << std::to_string(numIncomplete)
                << " incomplete flag ledgers. "
                   "This can happen, but is unlikely. Check indexer_key_shift "
                   "in config";
        }
        else
        {
            BOOST_LOG_TRIVIAL(info)
                << __func__ << " number of incomplete flag ledgers = "
                << std::to_string(numIncomplete);
        }
    }
}

std::optional<ripple::Fees>
BackendInterface::fetchFees(std::uint32_t seq) const
{
    ripple::Fees fees;

    auto key = ripple::keylet::fees().key;
    auto bytes = fetchLedgerObject(key, seq);

    if (!bytes)
    {
        BOOST_LOG_TRIVIAL(error) << __func__ << " - could not find fees";
        return {};
    }

    ripple::SerialIter it(bytes->data(), bytes->size());
    ripple::SLE sle{it, key};

    if (sle.getFieldIndex(ripple::sfBaseFee) != -1)
        fees.base = sle.getFieldU64(ripple::sfBaseFee);

    if (sle.getFieldIndex(ripple::sfReferenceFeeUnits) != -1)
        fees.units = sle.getFieldU32(ripple::sfReferenceFeeUnits);

    if (sle.getFieldIndex(ripple::sfReserveBase) != -1)
        fees.reserve = sle.getFieldU32(ripple::sfReserveBase);

    if (sle.getFieldIndex(ripple::sfReserveIncrement) != -1)
        fees.increment = sle.getFieldU32(ripple::sfReserveIncrement);

    return fees;
}

}  // namespace Backend
