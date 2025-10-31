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

#include "data/LedgerCache.hpp"

#include "data/Types.hpp"
#include "etlng/Models.hpp"
#include "util/Assert.hpp"

#include <xrpl/basics/base_uint.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <ostream>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace data {

uint32_t
LedgerCache::latestLedgerSequence() const
{
    std::shared_lock const lck{mtx_};
    return latestSeq_;
}

void
LedgerCache::waitUntilCacheContainsSeq(uint32_t seq)
{
    if (disabled_)
        return;

    std::unique_lock lock(mtx_);
    cv_.wait(lock, [this, seq] { return latestSeq_ >= seq; });
    return;
}

void
LedgerCache::update(std::vector<LedgerObject> const& objs, uint32_t seq, bool isBackground)
{
    if (disabled_)
        return;

    {
        std::scoped_lock const lck{mtx_};
        if (seq > latestSeq_) {
            ASSERT(
                seq == latestSeq_ + 1 || latestSeq_ == 0,
                "New sequence must be either next or first. seq = {}, latestSeq_ = {}",
                seq,
                latestSeq_
            );
            latestSeq_ = seq;
        }
        for (auto const& obj : objs) {
            if (!obj.blob.empty()) {
                if (isBackground && deletes_.contains(obj.key))
                    continue;

                auto& e = map_[obj.key];
                if (seq > e.seq) {
                    e = {.seq = seq, .blob = obj.blob};
                }
            } else {
                map_.erase(obj.key);
                if (!full_ && !isBackground)
                    deletes_.insert(obj.key);
            }
        }
        cv_.notify_all();
    }
}

void
LedgerCache::update(std::vector<etlng::model::Object> const& objs, uint32_t seq)
{
    if (disabled_)
        return;

    std::scoped_lock const lck{mtx_};
    if (seq > latestSeq_) {
        ASSERT(
            seq == latestSeq_ + 1 || latestSeq_ == 0,
            "New sequence must be either next or first. seq = {}, latestSeq_ = {}",
            seq,
            latestSeq_
        );
        latestSeq_ = seq;
    }

    deleted_.clear();  // previous update's deletes no longer needed

    for (auto const& obj : objs) {
        if (!obj.data.empty()) {
            auto& e = map_[obj.key];
            if (seq > e.seq)
                e = {.seq = seq, .blob = obj.data};
        } else {
            if (map_.contains(obj.key))
                deleted_[obj.key] = map_[obj.key];

            map_.erase(obj.key);
            if (!full_)
                deletes_.insert(obj.key);
        }
    }
    cv_.notify_all();
}

std::optional<LedgerObject>
LedgerCache::getSuccessor(ripple::uint256 const& key, uint32_t seq) const
{
    if (disabled_ or not full_)
        return {};

    std::shared_lock const lck{mtx_};
    ++successorReqCounter_.get();
    if (seq != latestSeq_)
        return {};
    auto e = map_.upper_bound(key);
    if (e == map_.end())
        return {};
    ++successorHitCounter_.get();
    return {{.key = e->first, .blob = e->second.blob}};
}

std::optional<LedgerObject>
LedgerCache::getPredecessor(ripple::uint256 const& key, uint32_t seq) const
{
    if (disabled_ or not full_)
        return {};

    std::shared_lock const lck{mtx_};
    if (seq != latestSeq_)
        return {};
    auto e = map_.lower_bound(key);
    if (e == map_.begin())
        return {};
    --e;
    return {{.key = e->first, .blob = e->second.blob}};
}

std::optional<Blob>
LedgerCache::get(ripple::uint256 const& key, uint32_t seq) const
{
    if (disabled_)
        return {};

    std::shared_lock const lck{mtx_};
    if (seq > latestSeq_)
        return {};
    ++objectReqCounter_.get();
    auto e = map_.find(key);
    if (e == map_.end())
        return {};
    if (seq < e->second.seq)
        return {};
    ++objectHitCounter_.get();
    return {e->second.blob};
}

std::optional<Blob>
LedgerCache::getDeleted(ripple::uint256 const& key, uint32_t seq) const
{
    if (disabled_)
        return std::nullopt;

    std::shared_lock const lck{mtx_};
    if (seq > latestSeq_)
        return std::nullopt;

    ++objectReqCounter_.get();

    auto e = deleted_.find(key);
    if (e == deleted_.end())
        return std::nullopt;

    if (seq < e->second.seq)
        return std::nullopt;

    ++objectHitCounter_.get();
    return {e->second.blob};
}

void
LedgerCache::setDisabled()
{
    disabled_ = true;
}

bool
LedgerCache::isDisabled() const
{
    return disabled_;
}

void
LedgerCache::setFull()
{
    if (disabled_)
        return;

    full_ = true;
    std::scoped_lock const lck{mtx_};
    deletes_.clear();
}

bool
LedgerCache::isFull() const
{
    return full_;
}

size_t
LedgerCache::size() const
{
    std::shared_lock const lck{mtx_};
    return map_.size();
}

float
LedgerCache::getObjectHitRate() const
{
    if (objectReqCounter_.get().value() == 0u)
        return 1;
    return static_cast<float>(objectHitCounter_.get().value()) / objectReqCounter_.get().value();
}

float
LedgerCache::getSuccessorHitRate() const
{
    if (successorReqCounter_.get().value() == 0u)
        return 1;
    return static_cast<float>(successorHitCounter_.get().value()) / successorReqCounter_.get().value();
}

void
log(std::chrono::steady_clock::time_point const& start, std::string_view message)
{
    auto const now = std::chrono::steady_clock::now();
    auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
    std::cout << elapsed << " ms: " << message << std::endl;
}

class Logger {
    std::chrono::steady_clock::time_point start_;

public:
    Logger() : start_(std::chrono::steady_clock::now())
    {
    }

    ~Logger()
    {
        log("done");
    }

    void
    log(std::string_view m) const
    {
        auto const now = std::chrono::steady_clock::now();
        auto const elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
        std::cout << elapsedMs << " ms: " << m << std::endl;
    }
};

void
LedgerCache::serialize()
{
    std::ofstream file("data.bin", std::ios::binary);
    if (not file.is_open()) {
        std::cerr << "error opening data.bin for writing" << std::endl;
        return;
    }

    struct Header {
        uint32_t version = 1;
        uint32_t latestSeq{};
        uint64_t mapSize{};
        uint64_t deletedSize{};
    };
    std::shared_lock lock{mtx_};

    Logger logger;
    Header const header{.latestSeq = latestSeq_, .mapSize = map_.size(), .deletedSize = deleted_.size()};
    file.write(reinterpret_cast<char const*>(&header), sizeof(header));
    logger.log("wrote header");

    for (auto const& [k, v] : map_) {
        file.write(reinterpret_cast<char const*>(k.data()), ripple::base_uint<256>::bytes);
        file.write(reinterpret_cast<char const*>(&v.seq), sizeof(v.seq));
        auto const blobSize = v.blob.size();
        file.write(reinterpret_cast<char const*>(&blobSize), sizeof(blobSize));
        file.write(reinterpret_cast<char const*>(v.blob.data()), v.blob.size());
    }
    file << std::string(16, 0);
    logger.log("wrote map");

    for (auto const& [k, v] : deleted_) {
        file.write(reinterpret_cast<char const*>(k.data()), ripple::base_uint<256>::bytes);
        file.write(reinterpret_cast<char const*>(&v.seq), sizeof(v.seq));
        auto const blobSize = v.blob.size();
        file.write(reinterpret_cast<char const*>(&blobSize), sizeof(blobSize));
        file.write(reinterpret_cast<char const*>(v.blob.data()), v.blob.size());
    }
    file << std::string(16, 0);
    logger.log("wrote deleted");
}

std::expected<LedgerCache, std::string>
LedgerCache::fromFile()
{
    std::ifstream file("data.bin", std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected("Failed to open data.bin for reading");
    }

    struct Header {
        uint32_t version = 1;
        uint32_t latestSeq{};
        uint64_t mapSize{};
        uint64_t deletedSize{};
    };

    Logger logger;
    Header header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!file) {
        return std::unexpected("Failed to read header from file");
    }

    if (header.version != 1) {
        return std::unexpected("Unsupported file version: " + std::to_string(header.version));
    }
    logger.log("read header");

    LedgerCache cache;
    cache.latestSeq_ = header.latestSeq;
    cache.full_ = true;  // Assume cache is full when loaded from file

    // Read main map
    for (uint64_t i = 0; i < header.mapSize; ++i) {
        ripple::uint256 key;
        file.read(reinterpret_cast<char*>(key.data()), ripple::base_uint<256>::bytes);
        if (!file) {
            return std::unexpected("Failed to read key from map at index " + std::to_string(i));
        }

        uint32_t seq{};
        file.read(reinterpret_cast<char*>(&seq), sizeof(seq));
        if (!file) {
            return std::unexpected("Failed to read sequence from map at index " + std::to_string(i));
        }

        size_t blobSize{};
        file.read(reinterpret_cast<char*>(&blobSize), sizeof(blobSize));
        if (!file) {
            return std::unexpected("Failed to read blob size from map at index " + std::to_string(i));
        }

        Blob blob;
        blob.resize(blobSize);
        file.read(reinterpret_cast<char*>(blob.data()), blobSize);
        if (!file) {
            return std::unexpected("Failed to read blob data from map at index " + std::to_string(i));
        }

        cache.map_.insert(cache.map_.end(), std::make_pair(key, CacheEntry{.seq = seq, .blob = std::move(blob)}));
    }

    // Read separator after map
    std::string separator(16, 0);
    file.read(separator.data(), 16);
    if (!file) {
        return std::unexpected("Failed to read separator after map");
    }

    // Verify separator is all zeros
    for (size_t i = 0; i < 16; ++i) {
        if (separator[i] != 0) {
            return std::unexpected(
                "Invalid separator after map: expected zeros but found non-zero byte at position " + std::to_string(i)
            );
        }
    }

    logger.log("read map");

    // Read deleted map
    for (uint64_t i = 0; i < header.deletedSize; ++i) {
        ripple::uint256 key;
        file.read(reinterpret_cast<char*>(key.data()), ripple::base_uint<256>::bytes);
        if (!file) {
            return std::unexpected("Failed to read key from deleted at index " + std::to_string(i));
        }

        uint32_t seq{};
        file.read(reinterpret_cast<char*>(&seq), sizeof(seq));
        if (!file) {
            return std::unexpected("Failed to read sequence from deleted at index " + std::to_string(i));
        }

        size_t blobSize{};
        file.read(reinterpret_cast<char*>(&blobSize), sizeof(blobSize));
        if (!file) {
            return std::unexpected("Failed to read blob size from deleted at index " + std::to_string(i));
        }

        Blob blob(blobSize);
        file.read(reinterpret_cast<char*>(blob.data()), blobSize);
        if (!file) {
            return std::unexpected("Failed to read blob data from deleted at index " + std::to_string(i));
        }

        cache.deleted_.insert(cache.map_.end(), std::make_pair(key, CacheEntry{.seq = seq, .blob = std::move(blob)}));
    }

    // Read final separator
    file.read(separator.data(), 16);
    if (!file) {
        return std::unexpected("Failed to read final separator");
    }

    // Verify final separator is all zeros
    for (size_t i = 0; i < 16; ++i) {
        if (separator[i] != 0) {
            return std::unexpected(
                "Invalid final separator: expected zeros but found non-zero byte at position " + std::to_string(i)
            );
        }
    }

    logger.log("read deleted");

    return cache;
}

LedgerCache::LedgerCache(LedgerCache&& other)
{
    *this = std::move(other);
}

LedgerCache&
LedgerCache::operator=(LedgerCache&& other)
{
    full_ = other.full_;
    map_ = std::move(other.map_);
    deleted_ = std::move(other.deleted_);
    latestSeq_ = other.latestSeq_;
    return *this;
}

}  // namespace data
