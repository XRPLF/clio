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

#include "data/BackendInterface.hpp"

#include "data/Types.hpp"
#include "util/Assert.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/spawn.hpp>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/strHex.h>
#include <ripple/protocol/Fees.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/Serializer.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// local to compilation unit loggers
namespace {
util::Logger gLog{"Backend"};
}  // namespace

/**
 * @brief This namespace implements the data access layer and related components.
 *
 * The data layer is responsible for fetching and storing data from the database.
 * Cassandra and ScyllaDB are currently supported via the `CassandraBackend` implementation.
 */
namespace data {
bool
BackendInterface::finishWrites(std::uint32_t const ledgerSequence)
{
    LOG(gLog.debug()) << "Want finish writes for " << ledgerSequence;
    auto commitRes = doFinishWrites();
    if (commitRes) {
        LOG(gLog.debug()) << "Successfully commited. Updating range now to " << ledgerSequence;
        updateRange(ledgerSequence);
    }
    return commitRes;
}
void
BackendInterface::writeLedgerObject(std::string&& key, std::uint32_t const seq, std::string&& blob)
{
    ASSERT(key.size() == sizeof(ripple::uint256), "Key must be 256 bits");
    doWriteLedgerObject(std::move(key), seq, std::move(blob));
}

std::optional<LedgerRange>
BackendInterface::hardFetchLedgerRangeNoThrow() const
{
    return retryOnTimeout([&]() { return hardFetchLedgerRange(); });
}

// *** state data methods
std::optional<Blob>
BackendInterface::fetchLedgerObject(
    ripple::uint256 const& key,
    std::uint32_t const sequence,
    boost::asio::yield_context yield
) const
{
    auto obj = cache_.get(key, sequence);
    if (obj) {
        LOG(gLog.trace()) << "Cache hit - " << ripple::strHex(key);
        return obj;
    }

    LOG(gLog.trace()) << "Cache miss - " << ripple::strHex(key);
    auto dbObj = doFetchLedgerObject(key, sequence, yield);
    if (!dbObj) {
        LOG(gLog.trace()) << "Missed cache and missed in db";
    } else {
        LOG(gLog.trace()) << "Missed cache but found in db";
    }
    return dbObj;
}

std::vector<Blob>
BackendInterface::fetchLedgerObjects(
    std::vector<ripple::uint256> const& keys,
    std::uint32_t const sequence,
    boost::asio::yield_context yield
) const
{
    std::vector<Blob> results;
    results.resize(keys.size());
    std::vector<ripple::uint256> misses;
    for (size_t i = 0; i < keys.size(); ++i) {
        auto obj = cache_.get(keys[i], sequence);
        if (obj) {
            results[i] = *obj;
        } else {
            misses.push_back(keys[i]);
        }
    }
    LOG(gLog.trace()) << "Cache hits = " << keys.size() - misses.size() << " - cache misses = " << misses.size();

    if (!misses.empty()) {
        auto objs = doFetchLedgerObjects(misses, sequence, yield);
        for (size_t i = 0, j = 0; i < results.size(); ++i) {
            if (results[i].empty()) {
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
    boost::asio::yield_context yield
) const
{
    auto succ = cache_.getSuccessor(key, ledgerSequence);
    if (succ) {
        LOG(gLog.trace()) << "Cache hit - " << ripple::strHex(key);
    } else {
        LOG(gLog.trace()) << "Cache miss - " << ripple::strHex(key);
    }
    return succ ? succ->key : doFetchSuccessorKey(key, ledgerSequence, yield);
}

std::optional<LedgerObject>
BackendInterface::fetchSuccessorObject(
    ripple::uint256 key,
    std::uint32_t const ledgerSequence,
    boost::asio::yield_context yield
) const
{
    auto succ = fetchSuccessorKey(key, ledgerSequence, yield);
    if (succ) {
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
    boost::asio::yield_context yield
) const
{
    // TODO try to speed this up. This can take a few seconds. The goal is
    // to get it down to a few hundred milliseconds.
    BookOffersPage page;
    ripple::uint256 const bookEnd = ripple::getQualityNext(book);
    ripple::uint256 uTipIndex = book;
    std::vector<ripple::uint256> keys;
    auto getMillis = [](auto diff) { return std::chrono::duration_cast<std::chrono::milliseconds>(diff).count(); };
    auto begin = std::chrono::system_clock::now();
    std::uint32_t numSucc = 0;
    std::uint32_t numPages = 0;
    long succMillis = 0;
    long pageMillis = 0;
    while (keys.size() < limit) {
        auto mid1 = std::chrono::system_clock::now();
        auto offerDir = fetchSuccessorObject(uTipIndex, ledgerSequence, yield);
        auto mid2 = std::chrono::system_clock::now();
        numSucc++;
        succMillis += getMillis(mid2 - mid1);
        if (!offerDir || offerDir->key >= bookEnd) {
            LOG(gLog.trace()) << "offerDir.has_value() " << offerDir.has_value() << " breaking";
            break;
        }
        uTipIndex = offerDir->key;
        while (keys.size() < limit) {
            ++numPages;
            ripple::STLedgerEntry const sle{
                ripple::SerialIter{offerDir->blob.data(), offerDir->blob.size()}, offerDir->key
            };
            auto indexes = sle.getFieldV256(ripple::sfIndexes);
            keys.insert(keys.end(), indexes.begin(), indexes.end());
            auto next = sle.getFieldU64(ripple::sfIndexNext);
            if (next == 0u) {
                LOG(gLog.trace()) << "Next is empty. breaking";
                break;
            }
            auto nextKey = ripple::keylet::page(uTipIndex, next);
            auto nextDir = fetchLedgerObject(nextKey.key, ledgerSequence, yield);
            ASSERT(nextDir.has_value(), "Next dir must exist");
            offerDir->blob = *nextDir;
            offerDir->key = nextKey.key;
        }
        auto mid3 = std::chrono::system_clock::now();
        pageMillis += getMillis(mid3 - mid2);
    }
    auto mid = std::chrono::system_clock::now();
    auto objs = fetchLedgerObjects(keys, ledgerSequence, yield);
    for (size_t i = 0; i < keys.size() && i < limit; ++i) {
        LOG(gLog.trace()) << "Key = " << ripple::strHex(keys[i]) << " blob = " << ripple::strHex(objs[i])
                          << " ledgerSequence = " << ledgerSequence;
        ASSERT(!objs[i].empty(), "Ledger object can't be empty");
        page.offers.push_back({keys[i], objs[i]});
    }
    auto end = std::chrono::system_clock::now();
    LOG(gLog.debug()) << "Fetching " << std::to_string(keys.size()) << " offers took "
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

std::optional<LedgerRange>
BackendInterface::hardFetchLedgerRange() const
{
    return synchronous([this](auto yield) { return hardFetchLedgerRange(yield); });
}

std::optional<LedgerRange>
BackendInterface::fetchLedgerRange() const
{
    std::shared_lock const lck(rngMtx_);
    return range;
}

void
BackendInterface::updateRange(uint32_t newMax)
{
    std::scoped_lock const lck(rngMtx_);

    ASSERT(
        !range || newMax >= range->maxSequence,
        "Range shouldn't exist yet or newMax should be greater. newMax = {}, range->maxSequence = {}",
        newMax,
        range->maxSequence
    );

    if (!range) {
        range = {newMax, newMax};
    } else {
        range->maxSequence = newMax;
    }
}

void
BackendInterface::setRange(uint32_t min, uint32_t max, bool force)
{
    std::scoped_lock const lck(rngMtx_);

    if (!force) {
        ASSERT(min <= max, "Range min must be less than or equal to max");
        ASSERT(not range.has_value(), "Range was already set");
    }

    range = {min, max};
}

LedgerPage
BackendInterface::fetchLedgerPage(
    std::optional<ripple::uint256> const& cursor,
    std::uint32_t const ledgerSequence,
    std::uint32_t const limit,
    bool outOfOrder,
    boost::asio::yield_context yield
)
{
    LedgerPage page;

    std::vector<ripple::uint256> keys;
    bool reachedEnd = false;

    while (keys.size() < limit && !reachedEnd) {
        ripple::uint256 const& curCursor = [&]() {
            if (!keys.empty())
                return keys.back();
            return (cursor ? *cursor : firstKey);
        }();

        std::uint32_t const seq = outOfOrder ? range->maxSequence : ledgerSequence;
        auto succ = fetchSuccessorKey(curCursor, seq, yield);

        if (!succ) {
            reachedEnd = true;
        } else {
            keys.push_back(*succ);
        }
    }

    auto objects = fetchLedgerObjects(keys, ledgerSequence, yield);
    for (size_t i = 0; i < objects.size(); ++i) {
        if (!objects[i].empty()) {
            page.objects.push_back({keys[i], std::move(objects[i])});
        } else if (!outOfOrder) {
            LOG(gLog.error()) << "Deleted or non-existent object in successor table. key = " << ripple::strHex(keys[i])
                              << " - seq = " << ledgerSequence;
            std::stringstream msg;
            for (size_t j = 0; j < objects.size(); ++j) {
                msg << " - " << ripple::strHex(keys[j]);
            }
            LOG(gLog.error()) << msg.str();

            if (corruptionDetector_.has_value())
                corruptionDetector_->onCorruptionDetected();
        }
    }
    if (!keys.empty() && !reachedEnd)
        page.cursor = keys.back();

    return page;
}

std::optional<ripple::Fees>
BackendInterface::fetchFees(std::uint32_t const seq, boost::asio::yield_context yield) const
{
    ripple::Fees fees;

    auto key = ripple::keylet::fees().key;
    auto bytes = fetchLedgerObject(key, seq, yield);

    if (!bytes) {
        LOG(gLog.error()) << "Could not find fees";
        return {};
    }

    ripple::SerialIter it(bytes->data(), bytes->size());
    ripple::SLE const sle{it, key};

    // XRPFees amendment introduced new fields for fees calculations.
    // New fields are set and the old fields are removed via `set_fees` tx.
    // Fallback to old fields if `set_fees` was not yet used to update the fields on this tx.
    auto hasNewFields = false;
    {
        auto const baseFeeXRP = sle.at(~ripple::sfBaseFeeDrops);
        auto const reserveBaseXRP = sle.at(~ripple::sfReserveBaseDrops);
        auto const reserveIncrementXRP = sle.at(~ripple::sfReserveIncrementDrops);

        if (baseFeeXRP)
            fees.base = baseFeeXRP->xrp();

        if (reserveBaseXRP)
            fees.reserve = reserveBaseXRP->xrp();

        if (reserveIncrementXRP)
            fees.increment = reserveIncrementXRP->xrp();

        hasNewFields = baseFeeXRP || reserveBaseXRP || reserveIncrementXRP;
    }

    if (not hasNewFields) {
        // Fallback to old fields
        auto const baseFee = sle.at(~ripple::sfBaseFee);
        auto const reserveBase = sle.at(~ripple::sfReserveBase);
        auto const reserveIncrement = sle.at(~ripple::sfReserveIncrement);

        if (baseFee)
            fees.base = baseFee.value();

        if (reserveBase)
            fees.reserve = reserveBase.value();

        if (reserveIncrement)
            fees.increment = reserveIncrement.value();
    }

    return fees;
}

}  // namespace data
