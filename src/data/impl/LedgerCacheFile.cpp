#include "data/impl/LedgerCacheFile.hpp"

#include "data/LedgerCache.hpp"
#include "data/Types.hpp"

#include <fmt/format.h>
#include <xrpl/basics/base_uint.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>

namespace data::impl {

using Hash = xrpl::uint256;
using Separator = std::array<char, 16>;
static constexpr Separator kSeparator = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

namespace {

std::expected<std::pair<xrpl::uint256, LedgerCache::CacheEntry>, std::string>
readCacheEntry(InputFile& file, size_t i)
{
    xrpl::uint256 key;
    if (not file.readRaw(reinterpret_cast<char*>(key.data()), xrpl::BaseUInt<256>::kBytes)) {
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

    Blob blob(blobSize);
    if (not file.readRaw(reinterpret_cast<char*>(blob.data()), blobSize)) {
        return std::unexpected(fmt::format("Failed to read blob data at index {}", i));
    }
    return std::make_pair(key, LedgerCache::CacheEntry{.seq = seq, .blob = std::move(blob)});
}

std::expected<void, std::string>
verifySeparator(Separator const& s)
{
    if (not std::ranges::all_of(s, [](char c) { return c == 0; })) {
        return std::unexpected{"Separator verification failed - data corruption detected"};
    }
    return {};
}

}  // anonymous namespace

LedgerCacheFile::LedgerCacheFile(std::string path) : path_(std::move(path))
{
}

std::expected<void, std::string>
LedgerCacheFile::write(DataView dataView)
{
    auto const newFilePath = fmt::format("{}.new", path_);
    auto file = OutputFile{newFilePath};
    if (not file.isOpen()) {
        return std::unexpected{fmt::format("Couldn't open file: {}", newFilePath)};
    }

    Header const header{
        .latestSeq = dataView.latestSeq,
        .mapSize = dataView.map.size(),
        .deletedSize = dataView.deleted.size()
    };
    file.write(header);
    file.write(kSeparator);

    for (auto const& [k, v] : dataView.map) {
        file.write(k.data(), decltype(k)::kBytes);
        file.write(v.seq);
        file.write(v.blob.size());
        file.writeRaw(reinterpret_cast<char const*>(v.blob.data()), v.blob.size());
    }
    file.write(kSeparator);

    for (auto const& [k, v] : dataView.deleted) {
        file.write(k.data(), decltype(k)::kBytes);
        file.write(v.seq);
        file.write(v.blob.size());
        file.writeRaw(reinterpret_cast<char const*>(v.blob.data()), v.blob.size());
    }
    file.write(kSeparator);
    auto const hash = file.hash();
    file.write(hash.data(), decltype(hash)::kBytes);

    // flush internal buffer explicitly before renaming
    if (auto const expectedSuccess = file.close(); not expectedSuccess.has_value()) {
        return expectedSuccess;
    }

    try {
        std::filesystem::rename(newFilePath, path_);
    } catch (std::exception const& e) {
        return std::unexpected{
            fmt::format("Error moving cache file from {} to {}: {}", newFilePath, path_, e.what())
        };
    }

    return {};
}

std::expected<LedgerCacheFile::Data, std::string>
LedgerCacheFile::read(uint32_t minLatestSequence)
{
    try {
        auto file = InputFile{path_};
        if (not file.isOpen()) {
            return std::unexpected{fmt::format("Couldn't open file: {}", path_)};
        }

        Data result;

        Header header{};
        if (not file.read(header)) {
            return std::unexpected{"Error reading cache header"};
        }
        if (header.version != kVersion) {
            return std::unexpected{fmt::format(
                "Cache has wrong version: expected {} found {}", kVersion, header.version
            )};
        }
        if (header.latestSeq < minLatestSequence) {
            return std::unexpected{
                fmt::format("Latest sequence ({}) in the cache file is too low.", header.latestSeq)
            };
        }
        result.latestSeq = header.latestSeq;

        Separator separator{};
        if (not file.readRaw(separator.data(), separator.size())) {
            return std::unexpected{"Error reading cache header"};
        }
        if (auto verificationResult = verifySeparator(separator);
            not verificationResult.has_value()) {
            return std::unexpected{std::move(verificationResult).error()};
        }

        for (size_t i = 0; i < header.mapSize; ++i) {
            auto cacheEntryExpected = readCacheEntry(file, i);
            if (not cacheEntryExpected.has_value()) {
                return std::unexpected{std::move(cacheEntryExpected).error()};
            }
            // Using insert with hint here to decrease insert operation complexity to the amortized
            // constant instead of logN
            result.map.insert(result.map.end(), std::move(cacheEntryExpected).value());
        }

        if (not file.readRaw(separator.data(), separator.size())) {
            return std::unexpected{"Error reading separator"};
        }
        if (auto verificationResult = verifySeparator(separator);
            not verificationResult.has_value()) {
            return std::unexpected{std::move(verificationResult).error()};
        }

        for (size_t i = 0; i < header.deletedSize; ++i) {
            auto cacheEntryExpected = readCacheEntry(file, i);
            if (not cacheEntryExpected.has_value()) {
                return std::unexpected{std::move(cacheEntryExpected).error()};
            }
            result.deleted.insert(result.deleted.end(), std::move(cacheEntryExpected).value());
        }

        if (not file.readRaw(separator.data(), separator.size())) {
            return std::unexpected{"Error reading separator"};
        }
        if (auto verificationResult = verifySeparator(separator);
            not verificationResult.has_value()) {
            return std::unexpected{std::move(verificationResult).error()};
        }

        auto const dataHash = file.hash();
        xrpl::uint256 hashFromFile{};
        if (not file.readRaw(
                reinterpret_cast<char*>(hashFromFile.data()), decltype(hashFromFile)::kBytes
            )) {
            return std::unexpected{"Error reading hash"};
        }

        if (dataHash != hashFromFile) {
            return std::unexpected{"Hash file corruption detected"};
        }

        return result;
    } catch (std::exception const& e) {
        return std::unexpected{fmt::format(" Error reading cache file: {}", e.what())};
    } catch (...) {
        return std::unexpected{fmt::format(" Error reading cache file")};
    }
}

}  // namespace data::impl
