//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#pragma once

#include "data/LedgerCache.hpp"
#include "data/Types.hpp"
#include "data/impl/InputFile.hpp"
#include "data/impl/OutputFile.hpp"
#include "util/Shasum.hpp"

#include <fmt/format.h>
#include <xrpl/basics/base_uint.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace data::impl {

class LedgerCacheFile {
public:
    struct Header {
        uint32_t version = kVERSION;
        uint64_t datetime{};
        uint32_t latestSeq{};
        uint64_t mapSize{};
        uint64_t deletedSize{};
    };

private:
    using Separator = std::array<char, 16>;
    std::string path_;
    bool isBuffered_;
    bool useCompression_;

    static constexpr uint32_t kVERSION = 1;
    static constexpr Separator kSEPARATOR = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    using Hash = ripple::uint256;

public:
    template <typename T>
    struct DataBase {
        uint32_t latestSeq{0};
        T map;
        T deleted;
    };

    using DataView = DataBase<LedgerCache::CacheMap const&>;
    using Data = DataBase<LedgerCache::CacheMap>;

    LedgerCacheFile(std::string path, bool isBuffered, bool useCompression)
        : path_(std::move(path)), isBuffered_(isBuffered), useCompression_(useCompression)
    {
    }

    std::expected<void, std::string>
    write(DataView dataView)
    {
        auto file = [&]() -> std::unique_ptr<OutputFile> {
            if (isBuffered_) {
                return std::make_unique<BufferedOutputFile>(path_, useCompression_, outputSize(dataView));
            }
            return std::make_unique<OutputFile>(path_, useCompression_);
        }();
        if (not file->isOpen()) {
            return std::unexpected{fmt::format("Couldn't open file: {}", path_)};
        }

        Header const header{
            .datetime = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()
            )
                            .count(),
            .latestSeq = dataView.latestSeq,
            .mapSize = dataView.map.size(),
            .deletedSize = dataView.deleted.size()
        };
        file->write(header);
        file->write(kSEPARATOR);

        for (auto const& [k, v] : dataView.map) {
            file->write(k.data(), decltype(k)::bytes);
            file->write(v.seq);
            file->write(v.blob.size());
            file->writeRaw(reinterpret_cast<char const*>(v.blob.data()), v.blob.size());
        }
        auto mapHash = calculateMapHash(dataView.map);
        file->write(mapHash.data(), decltype(mapHash)::bytes);
        file->write(kSEPARATOR);

        for (auto const& [k, v] : dataView.deleted) {
            file->write(k.data(), decltype(k)::bytes);
            file->write(v.seq);
            file->write(v.blob.size());
            file->writeRaw(reinterpret_cast<char const*>(v.blob.data()), v.blob.size());
        }
        auto deletedHash = calculateMapHash(dataView.deleted);
        file->write(deletedHash.data(), decltype(deletedHash)::bytes);
        file->write(kSEPARATOR);

        return {};
    }

    std::expected<Data, std::string>
    read()
    {
        try {
            auto file = [&]() -> std::unique_ptr<InputFile> {
                if (isBuffered_) {
                    return std::make_unique<BufferedInputFile>(path_, useCompression_);
                }
                return std::make_unique<InputFile>(path_, useCompression_);
            }();
            if (not file->isOpen()) {
                return std::unexpected{fmt::format("Couldn't open file: {}", path_)};
            }

            Data result;

            Header header{};
            if (not file->read(header)) {
                return std::unexpected{"Error reading cache header"};
            }
            if (header.version != kVERSION) {
                return std::unexpected{
                    fmt::format("Cache has wrong version: expected {} found {}", kVERSION, header.version)
                };
            }
            result.latestSeq = header.latestSeq;
            // TODO: check datetime or add sequence range

            Separator separator{};
            if (not file->readRaw(separator.data(), separator.size())) {
                return std::unexpected{"Error reading cache header"};
            }
            if (auto verificationResult = verifySeparator(separator); not verificationResult.has_value()) {
                return std::unexpected{std::move(verificationResult).error()};
            }

            for (size_t i = 0; i < header.mapSize; ++i) {
                auto cacheEntryExpected = readCacheEntry(*file, i);
                if (not cacheEntryExpected.has_value()) {
                    return std::unexpected{std::move(cacheEntryExpected).error()};
                }
                result.map.insert(result.map.end(), std::move(cacheEntryExpected).value());
            }

            Hash expectedMapHash;
            if (not file->readRaw(reinterpret_cast<char*>(expectedMapHash.data()), decltype(expectedMapHash)::bytes)) {
                return std::unexpected{"Error reading map hash"};
            }

            auto const actualMapHash = calculateMapHash(result.map);
            if (expectedMapHash != actualMapHash) {
                return std::unexpected{"Map hash verification failed - data corruption detected"};
            }

            if (not file->readRaw(separator.data(), separator.size())) {
                return std::unexpected{"Error reading separator"};
            }
            if (auto verificationResult = verifySeparator(separator); not verificationResult.has_value()) {
                return std::unexpected{std::move(verificationResult).error()};
            }

            for (size_t i = 0; i < header.deletedSize; ++i) {
                auto cacheEntryExpected = readCacheEntry(*file, i);
                if (not cacheEntryExpected.has_value()) {
                    return std::unexpected{std::move(cacheEntryExpected).error()};
                }
                result.deleted.insert(result.deleted.end(), std::move(cacheEntryExpected).value());
            }

            // Read and verify deleted hash
            Hash expectedDeletedHash;
            if (not file->readRaw(
                    reinterpret_cast<char*>(expectedDeletedHash.data()), decltype(expectedDeletedHash)::bytes
                )) {
                return std::unexpected{"Error reading deleted hash"};
            }

            auto const actualDeletedHash = calculateMapHash(result.deleted);
            if (expectedDeletedHash != actualDeletedHash) {
                return std::unexpected{"Deleted hash verification failed - data corruption detected"};
            }

            if (not file->readRaw(separator.data(), separator.size())) {
                return std::unexpected{"Error reading separator"};
            }
            if (auto verificationResult = verifySeparator(separator); not verificationResult.has_value()) {
                return std::unexpected{std::move(verificationResult).error()};
            }

            return result;
        } catch (std::exception const& e) {
            return std::unexpected{fmt::format(" Error reading cache file: {}", e.what())};
        } catch (...) {
            return std::unexpected{fmt::format(" Error reading cache file")};
        }
    }

private:
    static std::expected<std::pair<ripple::uint256, LedgerCache::CacheEntry>, std::string>
    readCacheEntry(InputFile& file, size_t i)
    {
        ripple::uint256 key;
        if (not file.readRaw(reinterpret_cast<char*>(key.data()), ripple::base_uint<256>::bytes)) {
            return std::unexpected(fmt::format("Failed to read key at index {}", i));
        }

        uint32_t seq{};
        if (not file.read(seq)) {
            return std::unexpected(fmt::format("Failed to read sequence at index {}", i));
        }

        size_t blobSize{};
        if (not file.read(blobSize)) {
            return std::unexpected(fmt::format("Failed to read blob size at index {}", i));
        }

        Blob blob;
        blob.resize(blobSize);
        if (not file.readRaw(reinterpret_cast<char*>(blob.data()), blobSize)) {
            return std::unexpected(fmt::format("Failed to read blob data at index {}", i));
        }
        return std::make_pair(key, LedgerCache::CacheEntry{.seq = seq, .blob = std::move(blob)});
    }

    static Hash
    calculateMapHash(LedgerCache::CacheMap const& map)
    {
        util::Sha256sum hasher;

        for (auto const& [key, entry] : map) {
            hasher.update(key.data(), decltype(key)::bytes);
            hasher.update(entry.seq);
            size_t const blobSize = entry.blob.size();
            hasher.update(blobSize);
            hasher.update(entry.blob.data(), entry.blob.size());
        }

        return std::move(hasher).finalize();
    }

    static std::expected<void, std::string>
    verifySeparator(Separator const& s)
    {
        if (not std::ranges::all_of(s, [](char c) { return c == 0; })) {
            return std::unexpected{"Separator verification failed - data corruption detected"};
        }
        return {};
    }

    static size_t
    outputSize(DataView dataView)
    {
        size_t size = sizeof(Header) + (4 * sizeof(Separator)) + (2 * sizeof(Hash));

        for (auto const& [k, v] : dataView.map) {
            size += decltype(k)::bytes + sizeof(v.seq) + sizeof(size_t) + v.blob.size();
        }

        for (auto const& [k, v] : dataView.deleted) {
            size += decltype(k)::bytes + sizeof(v.seq) + sizeof(size_t) + v.blob.size();
        }

        return size;
    }
};

}  // namespace data::impl
