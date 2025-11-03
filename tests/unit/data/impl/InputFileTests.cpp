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

#include "data/impl/InputFile.hpp"
#include "util/TmpFile.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace data::impl;

using InputFileTypes = ::testing::Types<InputFile, BufferedInputFile>;

template <typename T>
class InputFileTypedTest : public ::testing::Test {
protected:
    std::unique_ptr<InputFile>
    createInputFile(std::string const& path, bool useCompression = false)
    {
        return std::make_unique<T>(path, useCompression);
    }
};

struct NameGenerator {
    template <typename T>
    static std::string
    GetName(int)  // NOLINT(readability-identifier-naming)
    {
        if constexpr (std::is_same_v<T, InputFile>) {
            return "InputFile";
        } else if constexpr (std::is_same_v<T, BufferedInputFile>) {
            return "BufferedInputFile";
        } else {
            static_assert(false, "Unknown class");
        }
    }
};
TYPED_TEST_SUITE(InputFileTypedTest, InputFileTypes, NameGenerator);

TYPED_TEST(InputFileTypedTest, ConstructorWithValidFile)
{
    auto const tmpFile = TmpFile{"Hello, World!"};
    auto inputFile = this->createInputFile(tmpFile.path);

    EXPECT_TRUE(inputFile->isOpen());
}

TYPED_TEST(InputFileTypedTest, ConstructorWithInvalidFile)
{
    auto inputFile = this->createInputFile("/nonexistent/path/file.txt");

    EXPECT_FALSE(inputFile->isOpen());

    char i = 0;
    EXPECT_FALSE(inputFile->read(i));
    EXPECT_FALSE(inputFile->readRaw(&i, 1));
}

TYPED_TEST(InputFileTypedTest, ReadRawFromFile)
{
    std::string const content = "Test content for reading";
    auto tmpFile = TmpFile{content};
    auto inputFile = this->createInputFile(tmpFile.path);

    ASSERT_TRUE(inputFile->isOpen());

    std::vector<char> buffer(content.size());
    EXPECT_TRUE(inputFile->readRaw(buffer.data(), buffer.size()));
    EXPECT_EQ(std::string(buffer.data(), buffer.size()), content);
}

TYPED_TEST(InputFileTypedTest, ReadRawFromFilePartial)
{
    std::string content = "Hello, World!";
    auto tmpFile = TmpFile{content};
    auto inputFile = this->createInputFile(tmpFile.path, false);

    ASSERT_TRUE(inputFile->isOpen());

    std::vector<char> buffer(3);
    EXPECT_TRUE(inputFile->readRaw(buffer.data(), buffer.size()));
    EXPECT_EQ(std::string(buffer.data(), buffer.size()), "Hel");  // codespell:ignore

    buffer.resize(6);
    EXPECT_TRUE(inputFile->readRaw(buffer.data(), buffer.size()));
    EXPECT_EQ(std::string(buffer.data(), buffer.size()), "lo, Wo");

    buffer.resize(4);
    EXPECT_TRUE(inputFile->readRaw(buffer.data(), buffer.size()));
    EXPECT_EQ(std::string(buffer.data(), buffer.size()), "rld!");
}

TYPED_TEST(InputFileTypedTest, ReadRawAfterEnd)
{
    std::string content = "Test";
    auto tmpFile = TmpFile{content};
    auto inputFile = this->createInputFile(tmpFile.path);

    ASSERT_TRUE(inputFile->isOpen());

    std::vector<char> buffer(content.size());
    EXPECT_TRUE(inputFile->readRaw(buffer.data(), buffer.size()));

    char extraByte = 0;
    EXPECT_FALSE(inputFile->readRaw(&extraByte, 1));
}

TYPED_TEST(InputFileTypedTest, ReadRawFromFileExceedsSize)
{
    auto tmpFile = TmpFile{"Test"};
    auto inputFile = this->createInputFile(tmpFile.path, false);

    ASSERT_TRUE(inputFile->isOpen());

    std::vector<char> buffer(10);  // Larger than file content
    EXPECT_FALSE(inputFile->readRaw(buffer.data(), buffer.size()));
}

TYPED_TEST(InputFileTypedTest, ReadTemplateMethod)
{
    auto tmpFile = TmpFile{"\x01\x02\x03\x04"};
    auto inputFile = this->createInputFile(tmpFile.path, false);

    ASSERT_TRUE(inputFile->isOpen());

    std::uint32_t value{0};
    bool success = inputFile->read(value);

    EXPECT_TRUE(success);
    // Note: The actual value depends on endianness
    EXPECT_NE(value, 0u);
}

TYPED_TEST(InputFileTypedTest, ReadTemplateMethodFailure)
{
    auto tmpFile = TmpFile{"Hi"};  // Only 2 bytes
    auto inputFile = this->createInputFile(tmpFile.path, false);

    ASSERT_TRUE(inputFile->isOpen());

    std::uint64_t value{0};  // Trying to read 8 bytes
    EXPECT_FALSE(inputFile->read(value));
}

TYPED_TEST(InputFileTypedTest, ReadFromEmptyFile)
{
    auto tmpFile = TmpFile::empty();
    auto inputFile = this->createInputFile(tmpFile.path, false);

    ASSERT_TRUE(inputFile->isOpen());

    char byte = 0;
    EXPECT_FALSE(inputFile->readRaw(&byte, 1));
}
