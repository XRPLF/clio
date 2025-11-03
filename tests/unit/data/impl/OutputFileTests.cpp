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

#include "data/impl/OutputFile.hpp"
#include "util/MockAssert.hpp"
#include "util/TmpFile.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

using namespace data::impl;

template <typename T>
struct OutputFileTest : ::testing::Test {
    std::unique_ptr<T>
    createOutputFile()
    {
        if constexpr (std::is_same_v<T, OutputFile>) {
            return std::make_unique<OutputFile>(tmpFile.path, false);
        } else if constexpr (std::is_same_v<T, BufferedOutputFile>) {
            return std::make_unique<BufferedOutputFile>(tmpFile.path, false, 1024);
        }
    }

    std::string
    readFileContents()
    {
        std::ifstream ifs(tmpFile.path, std::ios::binary);
        return std::string{std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};
    }

    TmpFile tmpFile = TmpFile::empty();
};

// Type list for testing
using OutputFileTypes = ::testing::Types<OutputFile, BufferedOutputFile>;

// Name generator for typed tests
struct OutputFileTypeNames {
    template <typename T>
    static std::string
    GetName(int)  // NOLINT(readability-identifier-naming)
    {
        if constexpr (std::is_same_v<T, OutputFile>) {
            return "OutputFile";
        } else if constexpr (std::is_same_v<T, BufferedOutputFile>) {
            return "BufferedOutputFile";
        } else {
            static_assert(false, "Unknown class");
        }
    }
};

TYPED_TEST_SUITE(OutputFileTest, OutputFileTypes, OutputFileTypeNames);

TYPED_TEST(OutputFileTest, ConstructorOpensFile)
{
    auto const file = this->createOutputFile();
    EXPECT_TRUE(file->isOpen());
}

TYPED_TEST(OutputFileTest, NonExistingFile)
{
    std::string const invalidPath = "/invalid/nonexistent/directory/file.dat";

    if constexpr (std::is_same_v<TypeParam, OutputFile>) {
        auto file = std::make_unique<OutputFile>(invalidPath, false);
        EXPECT_FALSE(file->isOpen());
    } else if constexpr (std::is_same_v<TypeParam, BufferedOutputFile>) {
        auto file = std::make_unique<BufferedOutputFile>(invalidPath, false, 1024);
        EXPECT_FALSE(file->isOpen());
    }
}

TYPED_TEST(OutputFileTest, WriteBasicTypes)
{
    auto file = this->createOutputFile();

    // Test writing different basic types
    uint32_t const intValue = 0x12345678;
    double const doubleValue = std::numbers::pi;
    char const charValue = 'A';

    file->write(intValue);
    file->write(doubleValue);
    file->write(charValue);
    file.reset();

    std::string contents = this->readFileContents();
    EXPECT_EQ(contents.size(), sizeof(intValue) + sizeof(doubleValue) + sizeof(charValue));

    // Verify the data was written correctly
    auto* data = reinterpret_cast<char const*>(contents.data());
    EXPECT_EQ(*reinterpret_cast<uint32_t const*>(data), intValue);
    EXPECT_EQ(*reinterpret_cast<double const*>(data + sizeof(intValue)), doubleValue);
    EXPECT_EQ(*(data + sizeof(intValue) + sizeof(doubleValue)), charValue);
}

TYPED_TEST(OutputFileTest, WriteArray)
{
    auto file = this->createOutputFile();

    std::vector<uint32_t> data = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
    file->write(data.data(), data.size() * sizeof(uint32_t));
    file.reset();

    std::string contents = this->readFileContents();
    EXPECT_EQ(contents.size(), data.size() * sizeof(uint32_t));

    auto* readData = reinterpret_cast<uint32_t const*>(contents.data());
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(readData[i], data[i]);
    }
}

TYPED_TEST(OutputFileTest, WriteRawData)
{
    auto file = this->createOutputFile();

    std::string testData = "Hello, World!";
    file->writeRaw(testData.data(), testData.size());
    file.reset();

    std::string contents = this->readFileContents();
    EXPECT_EQ(contents, testData);
}

TYPED_TEST(OutputFileTest, WriteMultipleChunks)
{
    auto file = this->createOutputFile();

    std::string chunk1 = "First chunk";
    std::string chunk2 = "Second chunk";
    std::string chunk3 = "Third chunk";

    file->writeRaw(chunk1.data(), chunk1.size());
    file->writeRaw(chunk2.data(), chunk2.size());
    file->writeRaw(chunk3.data(), chunk3.size());
    file.reset();

    std::string contents = this->readFileContents();
    EXPECT_EQ(contents, chunk1 + chunk2 + chunk3);
}

struct BufferedOutputFileTest : common::util::WithMockAssert {
    std::string
    readFileContents() const
    {
        std::ifstream ifs(tmpFile.path, std::ios::binary);
        return std::string{std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};
    }

    TmpFile tmpFile = TmpFile::empty();
};

TEST_F(BufferedOutputFileTest, TooSmallBuffer)
{
    size_t const bufferSize = 5;
    auto file = std::make_unique<BufferedOutputFile>(tmpFile.path, false, bufferSize);

    std::string data = "This string is longer than buffer";

    EXPECT_CLIO_ASSERT_FAIL_WITH_MESSAGE(file->writeRaw(data.data(), data.size()), "Not enough space in buffer");
}

TEST_F(BufferedOutputFileTest, BufferSizeRespected)
{
    size_t const bufferSize = 10;
    auto file = std::make_unique<BufferedOutputFile>(tmpFile.path, false, bufferSize);

    std::string data = "12345";
    file->writeRaw(data.data(), data.size());

    // Data should still be in buffer
    std::string contents = readFileContents();
    EXPECT_TRUE(contents.empty());

    file.reset();
    contents = readFileContents();
    EXPECT_EQ(contents, data);
}

TEST_F(BufferedOutputFileTest, ExactBufferSize)
{
    size_t const bufferSize = 10;
    auto file = std::make_unique<BufferedOutputFile>(tmpFile.path, false, bufferSize);

    std::string data = "1234567890";  // Exactly buffer size
    ASSERT_EQ(data.size(), bufferSize);

    file->writeRaw(data.data(), data.size());

    // Data should still be in buffer
    std::string contents = readFileContents();
    EXPECT_TRUE(contents.empty());

    file.reset();
    contents = readFileContents();
    EXPECT_EQ(contents, data);
}

TEST_F(BufferedOutputFileTest, MultipleSmallWrites)
{
    size_t const bufferSize = 20;
    auto file = std::make_unique<BufferedOutputFile>(tmpFile.path, false, bufferSize);

    std::string part1 = "Hello";
    std::string part2 = " ";
    std::string part3 = "World!";

    file->writeRaw(part1.data(), part1.size());
    file->writeRaw(part2.data(), part2.size());
    file->writeRaw(part3.data(), part3.size());

    // Total size is 12, should fit in buffer of 20
    std::string contents = readFileContents();
    EXPECT_TRUE(contents.empty());

    file.reset();
    contents = readFileContents();
    EXPECT_EQ(contents, part1 + part2 + part3);
}

TEST_F(BufferedOutputFileTest, IncrementalBufferFill)
{
    size_t const bufferSize = 10;
    auto file = std::make_unique<BufferedOutputFile>(tmpFile.path, false, bufferSize);

    std::string part1 = "12345";  // 5 bytes
    std::string part2 = "67890";  // 5 bytes, total 10 (exact fit)

    file->writeRaw(part1.data(), part1.size());
    file->writeRaw(part2.data(), part2.size());

    // Now try to add one more byte - should fail
    char extraByte = 'X';
    EXPECT_CLIO_ASSERT_FAIL_WITH_MESSAGE(file->writeRaw(&extraByte, 1), "Not enough space in buffer");
}
