//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <backend/BackendInterface.h>
#include <log/Logger.h>

#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/STLedgerEntry.h>

using namespace clio;

// local to compilation unit loggers
namespace {
clio::Logger gLog{"Backend"};
}  // namespace

namespace Backend {
bool
BackendInterface::finishWrites(std::uint32_t const ledgerSequence)
{
    gLog.debug() << "Want finish writes for " << ledgerSequence;
    auto commitRes = doFinishWrites();
    if (commitRes)
    {
        gLog.debug() << "Successfully commited. Updating range now to " << ledgerSequence;
        updateRange(ledgerSequence);
    }
    return commitRes;
}
void
BackendInterface::writeLedgerObject(std::string&& key, std::uint32_t const seq, std::string&& blob)
{
    assert(key.size() == sizeof(ripple::uint256));
    doWriteLedgerObject(std::move(key), seq, std::move(blob));
}

std::optional<LedgerRange>
BackendInterface::hardFetchLedgerRangeNoThrow(boost::asio::yield_context& yield) const
{
    gLog.trace() << "called";
    while (true)
    {
        try
        {
            return hardFetchLedgerRange(yield);
        }
        catch (DatabaseTimeout& t)
        {
            ;
        }
    }
}

std::optional<LedgerRange>
BackendInterface::hardFetchLedgerRangeNoThrow() const
{
    gLog.trace() << "called";
    return retryOnTimeout([&]() { return hardFetchLedgerRange(); });
}

// *** state data methods
std::optional<Blob>
BackendInterface::fetchLedgerObject(
    ripple::uint256 const& key,
    std::uint32_t const sequence,
    boost::asio::yield_context& yield) const
{
    auto obj = cache_.get(key, sequence);
    if (obj)
    {
        gLog.trace() << "Cache hit - " << ripple::strHex(key);
        return *obj;
    }
    else
    {
        gLog.trace() << "Cache miss - " << ripple::strHex(key);
        auto dbObj = doFetchLedgerObject(key, sequence, yield);
        if (!dbObj)
            gLog.trace() << "Missed cache and missed in db";
        else
            gLog.trace() << "Missed cache but found in db";
        return dbObj;
    }
}

std::vector<Blob>
BackendInterface::fetchLedgerObjects(
    std::vector<ripple::uint256> const& keys,
    std::uint32_t const sequence,
    boost::asio::yield_context& yield) const
{
    std::vector<Blob> results;
    results.resize(keys.size());
    std::vector<ripple::uint256> misses;
    for (size_t i = 0; i < keys.size(); ++i)
    {
        auto obj = cache_.get(keys[i], sequence);
        if (obj)
            results[i] = *obj;
        else
            misses.push_back(keys[i]);
    }
    gLog.trace() << "Cache hits = " << keys.size() - misses.size() << " - cache misses = " << misses.size();

    if (misses.size())
    {
        auto objs = doFetchLedgerObjects(misses, sequence, yield);
        for (size_t i = 0, j = 0; i < results.size(); ++i)
        {
            if (results[i].size() == 0)
            {
                results[i] = objs[j];
                ++j;
            }
        }
    }

    return results;
}
// Fetches the successor to key/index
std::optional<ripple::uint256>
BackendInterface::fetchSuccessorKey(
    ripple::uint256 key,
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context& yield) const
{
    auto succ = cache_.getSuccessor(key, ledgerSequence);
    if (succ)
        gLog.trace() << "Cache hit - " << ripple::strHex(key);
    else
        gLog.trace() << "Cache miss - " << ripple::strHex(key);
    return succ ? succ->key : doFetchSuccessorKey(key, ledgerSequence, yield);
}

std::optional<LedgerObject>
BackendInterface::fetchSuccessorObject(
    ripple::uint256 key,
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context& yield) const
{
    auto succ = fetchSuccessorKey(key, ledgerSequence, yield);
    if (succ)
    {
        auto obj = fetchLedgerObject(*succ, ledgerSequence, yield);
        if (!obj)
            return {{*succ, {}}};

        return {{*succ, *obj}};
    }
    return {};
}

BookOffersPage
BackendInterface::fetchBookOffers(
    ripple::uint256 const& book,
    std::uint32_t const ledgerSequence,
    std::uint32_t const limit,
    boost::asio::yield_context& yield) const
{
    // TODO try to speed this up. This can take a few seconds. The goal is
    // to get it down to a few hundred milliseconds.
    BookOffersPage page;
    const ripple::uint256 bookEnd = ripple::getQualityNext(book);
    ripple::uint256 uTipIndex = book;
    std::vector<ripple::uint256> keys;
    auto getMillis = [](auto diff) { return std::chrono::duration_cast<std::chrono::milliseconds>(diff).count(); };
    auto begin = std::chrono::system_clock::now();
    std::uint32_t numSucc = 0;
    std::uint32_t numPages = 0;
    long succMillis = 0;
    long pageMillis = 0;
    while (keys.size() < limit)
    {
        auto mid1 = std::chrono::system_clock::now();
        auto offerDir = fetchSuccessorObject(uTipIndex, ledgerSequence, yield);
        auto mid2 = std::chrono::system_clock::now();
        numSucc++;
        succMillis += getMillis(mid2 - mid1);
        if (!offerDir || offerDir->key >= bookEnd)
        {
            gLog.trace() << "offerDir.has_value() " << offerDir.has_value() << " breaking";
            break;
        }
        uTipIndex = offerDir->key;
        while (keys.size() < limit)
        {
            ++numPages;
            ripple::STLedgerEntry sle{ripple::SerialIter{offerDir->blob.data(), offerDir->blob.size()}, offerDir->key};
            auto indexes = sle.getFieldV256(ripple::sfIndexes);
            keys.insert(keys.end(), indexes.begin(), indexes.end());
            auto next = sle.getFieldU64(ripple::sfIndexNext);
            if (!next)
            {
                gLog.trace() << "Next is empty. breaking";
                break;
            }
            auto nextKey = ripple::keylet::page(uTipIndex, next);
            auto nextDir = fetchLedgerObject(nextKey.key, ledgerSequence, yield);
            assert(nextDir);
            offerDir->blob = *nextDir;
            offerDir->key = nextKey.key;
        }
        auto mid3 = std::chrono::system_clock::now();
        pageMillis += getMillis(mid3 - mid2);
    }
    auto mid = std::chrono::system_clock::now();
    auto objs = fetchLedgerObjects(keys, ledgerSequence, yield);
    for (size_t i = 0; i < keys.size() && i < limit; ++i)
    {
        gLog.trace() << "Key = " << ripple::strHex(keys[i]) << " blob = " << ripple::strHex(objs[i])
                     << " ledgerSequence = " << ledgerSequence;
        assert(objs[i].size());
        page.offers.push_back({keys[i], objs[i]});
    }
    auto end = std::chrono::system_clock::now();
    gLog.debug() << "Fetching " << std::to_string(keys.size()) << " offers took "
                 << std::to_string(getMillis(mid - begin)) << " milliseconds. Fetching next dir took "
                 << std::to_string(succMillis) << " milliseonds. Fetched next dir " << std::to_string(numSucc)
                 << " times"
                 << " Fetching next page of dir took " << std::to_string(pageMillis) << " milliseconds"
                 << ". num pages = " << std::to_string(numPages) << ". Fetching all objects took "
                 << std::to_string(getMillis(end - mid))
                 << " milliseconds. total time = " << std::to_string(getMillis(end - begin)) << " milliseconds"
                 << " book = " << ripple::strHex(book);

    return page;
}

LedgerPage
BackendInterface::fetchLedgerPage(
    std::optional<ripple::uint256> const& cursor,
    std::uint32_t const ledgerSequence,
    std::uint32_t const limit,
    bool outOfOrder,
    boost::asio::yield_context& yield) const
{
    LedgerPage page;

    std::vector<ripple::uint256> keys;
    bool reachedEnd = false;
    while (keys.size() < limit && !reachedEnd)
    {
        ripple::uint256 const& curCursor = keys.size() ? keys.back() : cursor ? *cursor : firstKey;
        std::uint32_t const seq = outOfOrder ? range->maxSequence : ledgerSequence;
        auto succ = fetchSuccessorKey(curCursor, seq, yield);
        if (!succ)
            reachedEnd = true;
        else
            keys.push_back(std::move(*succ));
    }

    auto objects = fetchLedgerObjects(keys, ledgerSequence, yield);
    for (size_t i = 0; i < objects.size(); ++i)
    {
        if (objects[i].size())
            page.objects.push_back({std::move(keys[i]), std::move(objects[i])});
        else if (!outOfOrder)
        {
            gLog.error() << "Deleted or non-existent object in successor table. key = " << ripple::strHex(keys[i])
                         << " - seq = " << ledgerSequence;
            std::stringstream msg;
            for (size_t j = 0; j < objects.size(); ++j)
            {
                msg << " - " << ripple::strHex(keys[j]);
            }
            gLog.error() << msg.str();
        }
    }
    if (keys.size() && !reachedEnd)
        page.cursor = keys.back();

    return page;
}

std::optional<ripple::Fees>
BackendInterface::fetchFees(std::uint32_t const seq, boost::asio::yield_context& yield) const
{
    ripple::Fees fees;

    auto key = ripple::keylet::fees().key;
    auto bytes = fetchLedgerObject(key, seq, yield);

    if (!bytes)
    {
        gLog.error() << "Could not find fees";
        return {};
    }

    ripple::SerialIter it(bytes->data(), bytes->size());
    ripple::SLE sle{it, key};

    if (sle.getFieldIndex(ripple::sfBaseFee) != -1)
        fees.base = sle.getFieldU64(ripple::sfBaseFee);

    // if (sle.getFieldIndex(ripple::sfReferenceFeeUnits) != -1)
    //     fees.units = sle.getFieldU32(ripple::sfReferenceFeeUnits);

    if (sle.getFieldIndex(ripple::sfReserveBase) != -1)
        fees.reserve = sle.getFieldU32(ripple::sfReserveBase);

    if (sle.getFieldIndex(ripple::sfReserveIncrement) != -1)
        fees.increment = sle.getFieldU32(ripple::sfReserveIncrement);

    return fees;
}

}  // namespace Backend
