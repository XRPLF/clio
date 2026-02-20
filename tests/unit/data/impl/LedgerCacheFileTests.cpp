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

#include "data/LedgerCache.hpp"
#include "data/impl/LedgerCacheFile.hpp"
#include "util/NameGenerator.hpp"
#include "util/TmpFile.hpp"

#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace data::impl;

struct LedgerCacheFileTestBase : ::testing::Test {
    struct DataSizeParams {
        size_t mapEntries;
        size_t deletedEntries;
        size_t blobSize;
        std::string description;
    };

    enum class CorruptionType {
        InvalidVersion,
        CorruptedSeparator1,  // After header
        CorruptedSeparator2,  // After map hash
        CorruptedSeparator3,  // After deleted hash
        MapKeyCorrupted,
        MapSeqCorrupted,
        MapBlobSizeCorrupted,
        MapBlobDataCorrupted,
        DeletedKeyCorrupted,
        DeletedSeqCorrupted,
        DeletedBlobSizeCorrupted,
        DeletedBlobDataCorrupted,
        HeaderLatestSeqCorrupted
    };

    struct CorruptionParams {
        CorruptionType type;
        std::string description;
    };

    struct EntryOffsets {
        size_t keyOffset;
        size_t seqOffset;
        size_t blobSizeOffset;
        size_t blobDataOffset;
    };

    struct FileOffsets {
        size_t headerOffset;
        size_t separator1Offset;
        size_t mapStartOffset;
        std::vector<EntryOffsets> mapEntries;
        size_t separator2Offset;
        size_t deletedStartOffset;
        std::vector<EntryOffsets> deletedEntries;
        size_t separator3Offset;
        size_t hashOffset;

        static FileOffsets
        calculate(LedgerCacheFile::DataView const& dataView)
        {
            FileOffsets offsets{};
            size_t currentOffset = 0;

            offsets.headerOffset = currentOffset;
            currentOffset += sizeof(LedgerCacheFile::Header);

            offsets.separator1Offset = currentOffset;
            currentOffset += 16;

            // Map entries
            offsets.mapStartOffset = currentOffset;
            for (auto const& [key, entry] : dataView.map) {
                EntryOffsets entryOffsets{};
                entryOffsets.keyOffset = currentOffset;
                entryOffsets.seqOffset = currentOffset + 32;               // uint256 size
                entryOffsets.blobSizeOffset = currentOffset + 32 + 4;      // + uint32 size
                entryOffsets.blobDataOffset = currentOffset + 32 + 4 + 8;  // + size_t size

                offsets.mapEntries.push_back(entryOffsets);
                currentOffset += 32 + 4 + 8 + entry.blob.size();  // key + seq + size + blob
            }

            // Separator 2 (after map entries)
            offsets.separator2Offset = currentOffset;
            currentOffset += 16;

            // Deleted entries
            offsets.deletedStartOffset = currentOffset;
            for (auto const& [key, entry] : dataView.deleted) {
                EntryOffsets entryOffsets{};
                entryOffsets.keyOffset = currentOffset;
                entryOffsets.seqOffset = currentOffset + 32;
                entryOffsets.blobSizeOffset = currentOffset + 32 + 4;
                entryOffsets.blobDataOffset = currentOffset + 32 + 4 + 8;

                offsets.deletedEntries.push_back(entryOffsets);
                currentOffset += 32 + 4 + 8 + entry.blob.size();
            }

            // Separator 3 (after deleted entries)
            offsets.separator3Offset = currentOffset;
            currentOffset += 16;

            // Overall file hash
            offsets.hashOffset = currentOffset;

            return offsets;
        }
    };

    ~LedgerCacheFileTestBase() override
    {
        auto const pathWithNewPrefix = fmt::format("{}.new", tmpFile.path);
        if (std::filesystem::exists(pathWithNewPrefix))
            std::filesystem::remove(pathWithNewPrefix);
    }

    static std::vector<DataSizeParams> const kDATA_SIZE_PARAMS;
    static std::vector<CorruptionParams> const kCORRUPTION_PARAMS;

    TmpFile tmpFile = TmpFile::empty();
    static uint32_t constexpr kLATEST_SEQUENCE = 12345;

    static LedgerCacheFile::Data
    createTestData(size_t mapSize, size_t deletedSize, size_t blobSize)
    {
        LedgerCacheFile::Data data;
        data.latestSeq = kLATEST_SEQUENCE;

        for (size_t i = 0; i < mapSize; ++i) {
            ripple::uint256 key;
            std::memset(key.data(), static_cast<int>(i), ripple::uint256::size());

            data::LedgerCache::CacheEntry entry;
            entry.seq = static_cast<uint32_t>(1000 + i);
            entry.blob.resize(blobSize);
            std::memset(entry.blob.data(), static_cast<int>(i + 100), blobSize);

            data.map.emplace(key, std::move(entry));
        }

        for (size_t i = 0; i < deletedSize; ++i) {
            ripple::uint256 key;
            std::memset(key.data(), static_cast<int>(i + 200), ripple::uint256::size());

            data::LedgerCache::CacheEntry entry;
            entry.seq = static_cast<uint32_t>(2000 + i);
            entry.blob.resize(blobSize);
            std::memset(entry.blob.data(), static_cast<int>(i + 250), blobSize);

            data.deleted.emplace(key, std::move(entry));
        }

        return data;
    }

    static LedgerCacheFile::DataView
    toDataView(LedgerCacheFile::Data const& data)
    {
        return LedgerCacheFile::DataView{
            .latestSeq = data.latestSeq, .map = data.map, .deleted = data.deleted
        };
    }

    void
    corruptFile(CorruptionType type, LedgerCacheFile::DataView const& dataView) const
    {
        std::fstream file(tmpFile.path, std::ios::in | std::ios::out | std::ios::binary);
        ASSERT_TRUE(file.is_open());

        auto const offsets = FileOffsets::calculate(dataView);

        switch (type) {
            case CorruptionType::InvalidVersion:
                file.seekp(offsets.headerOffset);
                {
                    uint32_t invalidVersion = 999;
                    file.write(
                        reinterpret_cast<char const*>(&invalidVersion), sizeof(invalidVersion)
                    );
                }
                break;
            case CorruptionType::CorruptedSeparator1:
                file.seekp(offsets.separator1Offset);
                {
                    char const corruptByte = static_cast<char>(0xFF);
                    file.write(&corruptByte, 1);
                }
                break;
            case CorruptionType::CorruptedSeparator2:
                file.seekp(offsets.separator2Offset);
                {
                    char const corruptByte = static_cast<char>(0xFF);
                    file.write(&corruptByte, 1);
                }
                break;
            case CorruptionType::CorruptedSeparator3:
                file.seekp(offsets.separator3Offset);
                {
                    char const corruptByte = static_cast<char>(0xFF);
                    file.write(&corruptByte, 1);
                }
                break;
            case CorruptionType::MapKeyCorrupted:
                if (!offsets.mapEntries.empty()) {
                    file.seekp(offsets.mapEntries[0].keyOffset);
                    char const corruptByte = static_cast<char>(0xFF);
                    file.write(&corruptByte, 1);
                }
                break;
            case CorruptionType::MapSeqCorrupted:
                if (!offsets.mapEntries.empty()) {
                    file.seekp(offsets.mapEntries[0].seqOffset);
                    uint32_t const corruptSeq = std::numeric_limits<uint32_t>::max();
                    file.write(reinterpret_cast<char const*>(&corruptSeq), sizeof(corruptSeq));
                }
                break;
            case CorruptionType::MapBlobSizeCorrupted:
                if (!offsets.mapEntries.empty()) {
                    file.seekp(offsets.mapEntries[0].blobSizeOffset);
                    size_t const corruptSize = std::numeric_limits<size_t>::max();
                    file.write(reinterpret_cast<char const*>(&corruptSize), sizeof(corruptSize));
                }
                break;
            case CorruptionType::MapBlobDataCorrupted:
                if (!offsets.mapEntries.empty() && !dataView.map.begin()->second.blob.empty()) {
                    file.seekp(offsets.mapEntries[0].blobDataOffset);
                    char const corruptByte = static_cast<char>(0xFF);
                    file.write(&corruptByte, 1);
                }
                break;
            case CorruptionType::DeletedKeyCorrupted:
                if (!offsets.deletedEntries.empty()) {
                    file.seekp(offsets.deletedEntries[0].keyOffset);
                    char const corruptByte = static_cast<char>(0xFF);
                    file.write(&corruptByte, 1);
                }
                break;
            case CorruptionType::DeletedSeqCorrupted:
                if (!offsets.deletedEntries.empty()) {
                    file.seekp(offsets.deletedEntries[0].seqOffset);
                    uint32_t const corruptSeq = std::numeric_limits<uint32_t>::max();
                    file.write(reinterpret_cast<char const*>(&corruptSeq), sizeof(corruptSeq));
                }
                break;
            case CorruptionType::DeletedBlobSizeCorrupted:
                if (!offsets.deletedEntries.empty()) {
                    file.seekp(offsets.deletedEntries[0].blobSizeOffset);
                    size_t const corruptSize = std::numeric_limits<size_t>::max();
                    file.write(reinterpret_cast<char const*>(&corruptSize), sizeof(corruptSize));
                }
                break;
            case CorruptionType::DeletedBlobDataCorrupted:
                if (!offsets.deletedEntries.empty() &&
                    !dataView.deleted.begin()->second.blob.empty()) {
                    file.seekp(offsets.deletedEntries[0].blobDataOffset);
                    char const corruptByte = static_cast<char>(0xFF);
                    file.write(&corruptByte, 1);
                }
                break;
            case CorruptionType::HeaderLatestSeqCorrupted:
                file.seekp(offsets.headerOffset + sizeof(uint32_t));  // skip version
                {
                    uint32_t corruptSeq =
                        0;  // set to 0 to fail validation if minLatestSequence > 0
                    file.write(reinterpret_cast<char const*>(&corruptSeq), sizeof(corruptSeq));
                }
                break;
        }
    }

    static void
    verifyDataEquals(LedgerCacheFile::Data const& expected, LedgerCacheFile::Data const& actual)
    {
        EXPECT_EQ(expected.latestSeq, actual.latestSeq);
        EXPECT_EQ(expected.map.size(), actual.map.size());
        EXPECT_EQ(expected.deleted.size(), actual.deleted.size());

        for (auto const& [key, entry] : expected.map) {
            auto it = actual.map.find(key);
            ASSERT_NE(it, actual.map.end()) << "Key not found in actual map";
            EXPECT_EQ(entry.seq, it->second.seq);
            EXPECT_EQ(entry.blob, it->second.blob);
        }

        for (auto const& [key, entry] : expected.deleted) {
            auto it = actual.deleted.find(key);
            ASSERT_NE(it, actual.deleted.end()) << "Key not found in actual deleted";
            EXPECT_EQ(entry.seq, it->second.seq);
            EXPECT_EQ(entry.blob, it->second.blob);
        }
    }
};

std::vector<
    LedgerCacheFileTestBase::DataSizeParams> const LedgerCacheFileTestBase::kDATA_SIZE_PARAMS = {
    {.mapEntries = 0, .deletedEntries = 0, .blobSize = 0, .description = "empty"},
    {.mapEntries = 1, .deletedEntries = 0, .blobSize = 10, .description = "single_map_small_blob"},
    {.mapEntries = 0,
     .deletedEntries = 1,
     .blobSize = 100,
     .description = "single_deleted_medium_blob"},
    {.mapEntries = 5,
     .deletedEntries = 3,
     .blobSize = 1000,
     .description = "multiple_entries_large_blob"},
    {.mapEntries = 10,
     .deletedEntries = 10,
     .blobSize = 50000,
     .description = "many_entries_huge_blob"}
};

std::vector<LedgerCacheFileTestBase::CorruptionParams> const
    LedgerCacheFileTestBase::kCORRUPTION_PARAMS = {
        {.type = CorruptionType::InvalidVersion, .description = "invalid_version"},
        {.type = CorruptionType::CorruptedSeparator1, .description = "corrupted_separator1"},
        {.type = CorruptionType::CorruptedSeparator2, .description = "corrupted_separator2"},
        {.type = CorruptionType::CorruptedSeparator3, .description = "corrupted_separator3"},
        {.type = CorruptionType::MapKeyCorrupted, .description = "map_key_corrupted"},
        {.type = CorruptionType::MapSeqCorrupted, .description = "map_seq_corrupted"},
        {.type = CorruptionType::MapBlobSizeCorrupted, .description = "map_blob_size_corrupted"},
        {.type = CorruptionType::MapBlobDataCorrupted, .description = "map_blob_data_corrupted"},
        {.type = CorruptionType::DeletedKeyCorrupted, .description = "deleted_key_corrupted"},
        {.type = CorruptionType::DeletedSeqCorrupted, .description = "deleted_seq_corrupted"},
        {.type = CorruptionType::DeletedBlobSizeCorrupted,
         .description = "deleted_blob_size_corrupted"},
        {.type = CorruptionType::DeletedBlobDataCorrupted,
         .description = "deleted_blob_data_corrupted"},
        {.type = CorruptionType::HeaderLatestSeqCorrupted,
         .description = "header_latest_seq_corrupted"}
};

struct LedgerCacheFileTest
    : LedgerCacheFileTestBase,
      ::testing::WithParamInterface<LedgerCacheFileTestBase::DataSizeParams> {
    static std::string
    roundTripParamName(::testing::TestParamInfo<DataSizeParams> const& info)
    {
        return info.param.description;
    }
};

INSTANTIATE_TEST_SUITE_P(
    AllDataSizes,
    LedgerCacheFileTest,
    ::testing::ValuesIn(LedgerCacheFileTestBase::kDATA_SIZE_PARAMS),
    LedgerCacheFileTest::roundTripParamName
);

TEST_P(LedgerCacheFileTest, WriteAndReadData)
{
    auto dataParams = GetParam();
    LedgerCacheFile cacheFile(tmpFile.path);

    auto testData =
        createTestData(dataParams.mapEntries, dataParams.deletedEntries, dataParams.blobSize);
    auto dataView = toDataView(testData);

    auto writeResult = cacheFile.write(dataView);
    ASSERT_TRUE(writeResult.has_value()) << "Failed to write: " << writeResult.error();

    EXPECT_TRUE(std::filesystem::exists(tmpFile.path));
    EXPECT_GT(std::filesystem::file_size(tmpFile.path), 0u);

    auto readResult = cacheFile.read(0);
    ASSERT_TRUE(readResult.has_value()) << "Failed to read: " << readResult.error();

    verifyDataEquals(testData, readResult.value());
}

struct LedgerCacheFileCorruptionTest
    : LedgerCacheFileTestBase,
      ::testing::WithParamInterface<LedgerCacheFileTestBase::CorruptionParams> {
    static std::string
    corruptionParamName(::testing::TestParamInfo<CorruptionParams> const& info)
    {
        return info.param.description;
    }
};

INSTANTIATE_TEST_SUITE_P(
    AllCorruptions,
    LedgerCacheFileCorruptionTest,
    ::testing::ValuesIn(LedgerCacheFileTestBase::kCORRUPTION_PARAMS),
    LedgerCacheFileCorruptionTest::corruptionParamName
);

TEST_P(LedgerCacheFileCorruptionTest, HandleCorruption)
{
    auto corruptionParams = GetParam();
    LedgerCacheFile cacheFile(tmpFile.path);

    auto testData = createTestData(3, 2, 100);
    auto dataView = toDataView(testData);

    auto writeResult = cacheFile.write(dataView);
    ASSERT_TRUE(writeResult.has_value()) << "Failed to write: " << writeResult.error();

    corruptFile(corruptionParams.type, dataView);

    auto readResult = cacheFile.read(0);
    EXPECT_FALSE(readResult.has_value()) << "Should have failed to read corrupted file";

    std::string const& error = readResult.error();
    switch (corruptionParams.type) {
        case CorruptionType::InvalidVersion:
            EXPECT_THAT(error, ::testing::HasSubstr("wrong version"));
            break;
        case CorruptionType::CorruptedSeparator1:
        case CorruptionType::CorruptedSeparator2:
        case CorruptionType::CorruptedSeparator3:
            EXPECT_THAT(error, ::testing::HasSubstr("Separator verification failed"));
            break;
        case CorruptionType::MapKeyCorrupted:
        case CorruptionType::MapSeqCorrupted:
            EXPECT_FALSE(error.empty());
            break;
        case CorruptionType::MapBlobSizeCorrupted:
            EXPECT_THAT(
                error,
                ::testing::AnyOf(
                    ::testing::HasSubstr("Error reading cache file"),
                    ::testing::HasSubstr("Failed to read blob"),
                    ::testing::HasSubstr("Hash file corruption detected")
                )
            );
            break;
        case CorruptionType::MapBlobDataCorrupted:
            EXPECT_THAT(
                error,
                ::testing::AnyOf(
                    ::testing::HasSubstr("Hash file corruption detected"),
                    ::testing::HasSubstr("Error reading cache file")
                )
            );
            break;
        case CorruptionType::DeletedKeyCorrupted:
        case CorruptionType::DeletedSeqCorrupted:
            EXPECT_FALSE(error.empty());
            break;
        case CorruptionType::DeletedBlobSizeCorrupted:
            EXPECT_THAT(
                error,
                ::testing::AnyOf(
                    ::testing::HasSubstr("Error reading cache file"),
                    ::testing::HasSubstr("Failed to read blob"),
                    ::testing::HasSubstr("Hash file corruption detected")
                )
            );
            break;
        case CorruptionType::DeletedBlobDataCorrupted:
            EXPECT_THAT(
                error,
                ::testing::AnyOf(
                    ::testing::HasSubstr("Hash file corruption detected"),
                    ::testing::HasSubstr("Error reading cache file")
                )
            );
            break;
        case CorruptionType::HeaderLatestSeqCorrupted:
            EXPECT_THAT(error, ::testing::HasSubstr("Hash file corruption detected"));
            break;
    }
}

struct LedgerCacheFileEdgeCaseTest : LedgerCacheFileTestBase {};

TEST_F(LedgerCacheFileEdgeCaseTest, NonExistingFile)
{
    LedgerCacheFile invalidPathFile("/invalid/path/file.cache");

    auto testData = createTestData(1, 1, 10);
    auto dataView = toDataView(testData);

    auto writeResult = invalidPathFile.write(dataView);
    EXPECT_FALSE(writeResult.has_value());
    EXPECT_THAT(writeResult.error(), ::testing::HasSubstr("Couldn't open file"));

    auto readResult = invalidPathFile.read(0);
    EXPECT_FALSE(readResult.has_value());
    EXPECT_THAT(readResult.error(), ::testing::HasSubstr("Couldn't open file"));
}

TEST_F(LedgerCacheFileEdgeCaseTest, MaxSequenceNumber)
{
    LedgerCacheFile cacheFile(tmpFile.path);

    auto testData = createTestData(1, 1, 10);
    testData.latestSeq = std::numeric_limits<uint32_t>::max();
    auto dataView = toDataView(testData);

    auto writeResult = cacheFile.write(dataView);
    ASSERT_TRUE(writeResult.has_value());

    auto readResult = cacheFile.read(0);
    ASSERT_TRUE(readResult.has_value());

    verifyDataEquals(testData, readResult.value());
}

TEST_F(LedgerCacheFileEdgeCaseTest, ZeroSizedBlobs)
{
    LedgerCacheFile cacheFile(tmpFile.path);

    auto testData = createTestData(3, 2, 0);
    auto dataView = toDataView(testData);

    auto writeResult = cacheFile.write(dataView);
    ASSERT_TRUE(writeResult.has_value());

    auto readResult = cacheFile.read(0);
    ASSERT_TRUE(readResult.has_value());

    verifyDataEquals(testData, readResult.value());
}

TEST_F(LedgerCacheFileEdgeCaseTest, SpecialKeyPatterns)
{
    LedgerCacheFile cacheFile(tmpFile.path);

    LedgerCacheFile::Data testData;
    testData.latestSeq = 100;

    ripple::uint256 zeroKey;
    std::memset(zeroKey.data(), 0, ripple::uint256::size());
    testData.map.emplace(zeroKey, data::LedgerCache::CacheEntry{.seq = 1, .blob = {1, 2, 3}});

    ripple::uint256 onesKey;
    std::memset(onesKey.data(), 0xFF, ripple::uint256::size());
    testData.map.emplace(onesKey, data::LedgerCache::CacheEntry{.seq = 2, .blob = {4, 5, 6}});

    ripple::uint256 altKey;
    for (size_t i = 0; i < ripple::uint256::size(); ++i) {
        altKey.data()[i] = static_cast<unsigned char>(((i % 2) != 0u) ? 0xAA : 0x55);
    }
    testData.deleted.emplace(altKey, data::LedgerCache::CacheEntry{.seq = 3, .blob = {7, 8, 9}});

    auto dataView = toDataView(testData);

    auto writeResult = cacheFile.write(dataView);
    ASSERT_TRUE(writeResult.has_value());

    auto readResult = cacheFile.read(0);
    ASSERT_TRUE(readResult.has_value());

    verifyDataEquals(testData, readResult.value());
}

TEST_F(LedgerCacheFileEdgeCaseTest, LargeBlobs)
{
    LedgerCacheFile cacheFile(tmpFile.path);

    auto testData = createTestData(1, 1, 1024 * 1024);
    auto dataView = toDataView(testData);

    auto writeResult = cacheFile.write(dataView);
    ASSERT_TRUE(writeResult.has_value());

    auto readResult = cacheFile.read(0);
    ASSERT_TRUE(readResult.has_value());

    verifyDataEquals(testData, readResult.value());
}

TEST_F(LedgerCacheFileEdgeCaseTest, SequenceNumber)
{
    LedgerCacheFile cacheFile(tmpFile.path);

    LedgerCacheFile::Data testData;
    testData.latestSeq = 0;

    ripple::uint256 key1, key2, key3;
    std::memset(key1.data(), 1, ripple::uint256::size());
    std::memset(key2.data(), 2, ripple::uint256::size());
    std::memset(key3.data(), 3, ripple::uint256::size());

    testData.map.emplace(key1, data::LedgerCache::CacheEntry{.seq = 0, .blob = {1}});
    testData.map.emplace(
        key2,
        data::LedgerCache::CacheEntry{.seq = std::numeric_limits<uint32_t>::max(), .blob = {2}}
    );
    testData.deleted.emplace(
        key3,
        data::LedgerCache::CacheEntry{.seq = std::numeric_limits<uint32_t>::max() / 2, .blob = {3}}
    );

    auto dataView = toDataView(testData);

    auto writeResult = cacheFile.write(dataView);
    ASSERT_TRUE(writeResult.has_value());

    auto readResult = cacheFile.read(0);
    ASSERT_TRUE(readResult.has_value());

    verifyDataEquals(testData, readResult.value());
}

TEST_F(LedgerCacheFileEdgeCaseTest, OnlyMapEntries)
{
    LedgerCacheFile cacheFile(tmpFile.path);

    auto testData = createTestData(5, 0, 100);
    auto dataView = toDataView(testData);

    auto writeResult = cacheFile.write(dataView);
    ASSERT_TRUE(writeResult.has_value());

    auto readResult = cacheFile.read(0);
    ASSERT_TRUE(readResult.has_value());

    verifyDataEquals(testData, readResult.value());
}

TEST_F(LedgerCacheFileEdgeCaseTest, OnlyDeletedEntries)
{
    LedgerCacheFile cacheFile(tmpFile.path);

    auto testData = createTestData(0, 5, 100);
    auto dataView = toDataView(testData);

    auto writeResult = cacheFile.write(dataView);
    ASSERT_TRUE(writeResult.has_value());

    auto readResult = cacheFile.read(0);
    ASSERT_TRUE(readResult.has_value());

    verifyDataEquals(testData, readResult.value());
}

TEST_F(LedgerCacheFileEdgeCaseTest, WriteCreatesFileWithSuffixNew)
{
    // The test causes failure of rename operation by creating destination as directory
    std::filesystem::remove(tmpFile.path);
    std::filesystem::create_directory(tmpFile.path);

    LedgerCacheFile cacheFile(tmpFile.path);
    auto testData = createTestData(1, 1, 10);
    auto dataView = toDataView(testData);

    auto writeResult = cacheFile.write(dataView);

    EXPECT_FALSE(writeResult.has_value());
    auto newFilePath = fmt::format("{}.new", tmpFile.path);
    EXPECT_THAT(writeResult.error(), ::testing::HasSubstr(newFilePath));

    EXPECT_TRUE(std::filesystem::exists(newFilePath));
    EXPECT_TRUE(std::filesystem::is_regular_file(newFilePath));
}

struct LedgerCacheFileMinSequenceValidationParams {
    uint32_t latestSeq;
    uint32_t minLatestSeq;
    bool shouldSucceed;
    std::string testName;
};

struct LedgerCacheFileMinSequenceValidationTest
    : LedgerCacheFileTestBase,
      ::testing::WithParamInterface<LedgerCacheFileMinSequenceValidationParams> {};

INSTANTIATE_TEST_SUITE_P(
    LedgerCacheFileMinSequenceValidationTests,
    LedgerCacheFileMinSequenceValidationTest,
    ::testing::Values(
        LedgerCacheFileMinSequenceValidationParams{
            .latestSeq = 1000u,
            .minLatestSeq = 500u,
            .shouldSucceed = true,
            .testName = "accept_when_min_less_than_latest"
        },
        LedgerCacheFileMinSequenceValidationParams{
            .latestSeq = 1000u,
            .minLatestSeq = 2000u,
            .shouldSucceed = false,
            .testName = "reject_when_min_greater_than_latest"
        },
        LedgerCacheFileMinSequenceValidationParams{
            .latestSeq = 1000u,
            .minLatestSeq = 1000u,
            .shouldSucceed = true,
            .testName = "accept_when_min_equals_latest"
        },
        LedgerCacheFileMinSequenceValidationParams{
            .latestSeq = 0u,
            .minLatestSeq = 0u,
            .shouldSucceed = true,
            .testName = "accept_zero_sequence"
        }
    ),
    tests::util::kNAME_GENERATOR
);

TEST_P(LedgerCacheFileMinSequenceValidationTest, ValidateMinSequence)
{
    auto const params = GetParam();
    auto const latestSeq = params.latestSeq;
    auto const minLatestSeq = params.minLatestSeq;
    auto const shouldSucceed = params.shouldSucceed;

    LedgerCacheFile cacheFile(tmpFile.path);
    auto testData = createTestData(3, 2, 100);
    testData.latestSeq = latestSeq;
    auto dataView = toDataView(testData);

    auto writeResult = cacheFile.write(dataView);
    ASSERT_TRUE(writeResult.has_value());

    auto readResult = cacheFile.read(minLatestSeq);

    if (shouldSucceed) {
        ASSERT_TRUE(readResult.has_value())
            << "Expected read to succeed but got error: " << readResult.error();
        EXPECT_EQ(readResult.value().latestSeq, latestSeq);
    } else {
        EXPECT_FALSE(readResult.has_value()) << "Expected read to fail but it succeeded";
        EXPECT_THAT(readResult.error(), ::testing::HasSubstr("too low"));
    }
}
