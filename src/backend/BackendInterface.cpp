#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <backend/BackendInterface.h>
namespace Backend {
bool
BackendInterface::finishWrites(uint32_t ledgerSequence) const
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
    std::string&& blob,
    bool isCreated,
    bool isDeleted,
    std::optional<ripple::uint256>&& book) const
{
    ripple::uint256 key256 = ripple::uint256::fromVoid(key.data());
    indexer_.addKey(std::move(key256));
    doWriteLedgerObject(
        std::move(key),
        seq,
        std::move(blob),
        isCreated,
        isDeleted,
        std::move(book));
}
std::optional<LedgerRange>
BackendInterface::fetchLedgerRangeNoThrow() const
{
    BOOST_LOG_TRIVIAL(warning) << __func__;
    while (true)
    {
        try
        {
            return fetchLedgerRange();
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
            // TODO we probably don't have to wait here. We can probably fetch
            // these objects in another thread, and move on to another page of
            // the book directory, or another directory. We also could just
            // accumulate all of the keys before fetching the offers
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
    for (size_t i = 0; i < keys.size(); ++i)
    {
        BOOST_LOG_TRIVIAL(trace)
            << __func__ << " key = " << ripple::strHex(keys[i])
            << " blob = " << ripple::strHex(objs[i]);
        assert(objs[i].size());
        page.offers.push_back({keys[i], objs[i]});
    }
    auto end = std::chrono::system_clock::now();
    BOOST_LOG_TRIVIAL(info)
        << __func__ << " "
        << "Fetching " << std::to_string(keys.size()) << " keys took "
        << std::to_string(getMillis(mid - begin))
        << " milliseconds. Fetching next dir took "
        << std::to_string(succMillis) << " milliseonds. Fetched next dir "
        << std::to_string(numSucc) << " times"
        << " Fetching next page of dir took " << std::to_string(pageMillis)
        << ". num pages = " << std::to_string(numPages)
        << " milliseconds. Fetching all objects took "
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
    // really low limits almost always miss
    uint32_t adjustedLimit = std::max(limitHint, std::max(limit, (uint32_t)4));
    LedgerPage page;
    page.cursor = cursor;
    do
    {
        adjustedLimit = adjustedLimit >= 8192 ? 8192 : adjustedLimit * 2;
        auto start = std::chrono::system_clock::now();
        auto partial =
            doFetchLedgerPage(page.cursor, ledgerSequence, adjustedLimit);
        auto end = std::chrono::system_clock::now();
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " " << std::to_string(ledgerSequence) << " "
            << std::to_string(adjustedLimit) << " "
            << ripple::strHex(*page.cursor) << " - time = "
            << std::to_string(
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       end - start)
                       .count());
        page.objects.insert(
            page.objects.end(), partial.objects.begin(), partial.objects.end());
        page.cursor = partial.cursor;
    } while (page.objects.size() < limit && page.cursor);
    
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
        page.warning = "Data may be incomplete";
    }
    if (page.objects.size() >= limit)
    {
        page.objects.resize(limit);
        page.cursor = page.objects.back().key;
    }
    return page;
}

void
BackendInterface::checkFlagLedgers() const
{
    auto rng = fetchLedgerRangeNoThrow();
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
}  // namespace Backend
