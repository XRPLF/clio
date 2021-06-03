#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <reporting/BackendInterface.h>
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
    BookOffersPage page;
    const ripple::uint256 bookEnd = ripple::getQualityNext(book);
    ripple::uint256 uTipIndex = book;
    bool done = false;
    while (page.offers.size() < limit)
    {
        auto offerDir = fetchSuccessor(uTipIndex, ledgerSequence);
        if (!offerDir || offerDir->key > bookEnd)
        {
            BOOST_LOG_TRIVIAL(debug) << __func__ << " - offerDir.has_value() "
                                     << offerDir.has_value() << " breaking";
            break;
        }
        while (page.offers.size() < limit)
        {
            uTipIndex = offerDir->key;
            ripple::STLedgerEntry sle{
                ripple::SerialIter{
                    offerDir->blob.data(), offerDir->blob.size()},
                offerDir->key};
            auto indexes = sle.getFieldV256(ripple::sfIndexes);
            std::vector<ripple::uint256> keys;
            keys.insert(keys.end(), indexes.begin(), indexes.end());
            auto objs = fetchLedgerObjects(keys, ledgerSequence);
            for (size_t i = 0; i < keys.size(); ++i)
            {
                BOOST_LOG_TRIVIAL(debug)
                    << __func__ << " key = " << ripple::strHex(keys[i])
                    << " blob = " << ripple::strHex(objs[i]);
                if (objs[i].size())
                    page.offers.push_back({keys[i], objs[i]});
            }
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
    }

    return page;
}

std::optional<LedgerObject>
BackendInterface::fetchSuccessor(ripple::uint256 key, uint32_t ledgerSequence)
    const
{
    auto page = fetchLedgerPage({++key}, ledgerSequence, 1);
    if (page.objects.size())
        return page.objects[0];
    return {};
}
LedgerPage
BackendInterface::fetchLedgerPage(
    std::optional<ripple::uint256> const& cursor,
    std::uint32_t ledgerSequence,
    std::uint32_t limit) const
{
    assert(limit != 0);
    bool incomplete = !isLedgerIndexed(ledgerSequence);
    // really low limits almost always miss
    uint32_t adjustedLimit = std::max(limit, (uint32_t)4);
    LedgerPage page;
    page.cursor = cursor;
    do
    {
        adjustedLimit = adjustedLimit > 2048 ? 2048 : adjustedLimit * 2;
        auto partial =
            doFetchLedgerPage(page.cursor, ledgerSequence, adjustedLimit);
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
