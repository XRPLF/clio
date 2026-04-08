#include "data/impl/InputFile.hpp"
#include "util/Shasum.hpp"
#include "util/TmpFile.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace data::impl;

struct InputFileTest : ::testing::Test {};

TEST_F(InputFileTest, ConstructorWithValidFile)
{
    auto const tmpFile = TmpFile{"Hello, World!"};
    InputFile const inputFile(tmpFile.path);

    EXPECT_TRUE(inputFile.isOpen());
}

TEST_F(InputFileTest, ConstructorWithInvalidFile)
{
    InputFile inputFile("/nonexistent/path/file.txt");

    EXPECT_FALSE(inputFile.isOpen());

    char i = 0;
    EXPECT_FALSE(inputFile.read(i));
    EXPECT_FALSE(inputFile.readRaw(&i, 1));
}

TEST_F(InputFileTest, ReadRawFromFile)
{
    std::string const content = "Test content for reading";
    auto tmpFile = TmpFile{content};
    InputFile inputFile(tmpFile.path);

    ASSERT_TRUE(inputFile.isOpen());

    std::vector<char> buffer(content.size());
    EXPECT_TRUE(inputFile.readRaw(buffer.data(), buffer.size()));
    EXPECT_EQ(std::string(buffer.data(), buffer.size()), content);
}

TEST_F(InputFileTest, ReadRawFromFilePartial)
{
    std::string const content = "Hello, World!";
    auto tmpFile = TmpFile{content};
    InputFile inputFile(tmpFile.path);

    ASSERT_TRUE(inputFile.isOpen());

    std::vector<char> buffer(3);
    EXPECT_TRUE(inputFile.readRaw(buffer.data(), buffer.size()));
    EXPECT_EQ(std::string(buffer.data(), buffer.size()), "Hel");  // codespell:ignore

    buffer.resize(6);
    EXPECT_TRUE(inputFile.readRaw(buffer.data(), buffer.size()));
    EXPECT_EQ(std::string(buffer.data(), buffer.size()), "lo, Wo");

    buffer.resize(4);
    EXPECT_TRUE(inputFile.readRaw(buffer.data(), buffer.size()));
    EXPECT_EQ(std::string(buffer.data(), buffer.size()), "rld!");
}

TEST_F(InputFileTest, ReadRawAfterEnd)
{
    std::string const content = "Test";
    auto tmpFile = TmpFile{content};
    InputFile inputFile(tmpFile.path);

    ASSERT_TRUE(inputFile.isOpen());

    std::vector<char> buffer(content.size());
    EXPECT_TRUE(inputFile.readRaw(buffer.data(), buffer.size()));

    char extraByte = 0;
    EXPECT_FALSE(inputFile.readRaw(&extraByte, 1));
}

TEST_F(InputFileTest, ReadRawFromFileExceedsSize)
{
    auto tmpFile = TmpFile{"Test"};
    InputFile inputFile(tmpFile.path);

    ASSERT_TRUE(inputFile.isOpen());

    std::vector<char> buffer(10);  // Larger than file content
    EXPECT_FALSE(inputFile.readRaw(buffer.data(), buffer.size()));
}

TEST_F(InputFileTest, ReadTemplateMethod)
{
    auto tmpFile = TmpFile{"\x01\x02\x03\x04"};
    InputFile inputFile(tmpFile.path);

    ASSERT_TRUE(inputFile.isOpen());

    std::uint32_t value{0};
    bool const success = inputFile.read(value);

    EXPECT_TRUE(success);
    // Note: The actual value depends on endianness
    EXPECT_NE(value, 0u);
}

TEST_F(InputFileTest, ReadTemplateMethodFailure)
{
    auto tmpFile = TmpFile{"Hi"};  // Only 2 bytes
    InputFile inputFile(tmpFile.path);

    ASSERT_TRUE(inputFile.isOpen());

    std::uint64_t value{0};  // Trying to read 8 bytes
    EXPECT_FALSE(inputFile.read(value));
}

TEST_F(InputFileTest, ReadFromEmptyFile)
{
    auto tmpFile = TmpFile::empty();
    InputFile inputFile(tmpFile.path);

    ASSERT_TRUE(inputFile.isOpen());

    char byte = 0;
    EXPECT_FALSE(inputFile.readRaw(&byte, 1));
}

TEST_F(InputFileTest, HashOfEmptyFile)
{
    auto tmpFile = TmpFile::empty();
    InputFile const inputFile(tmpFile.path);

    ASSERT_TRUE(inputFile.isOpen());
    EXPECT_EQ(inputFile.hash(), util::sha256sum(""));
}

TEST_F(InputFileTest, HashAfterReading)
{
    std::string const content = "Hello, World!";
    auto tmpFile = TmpFile{content};
    InputFile inputFile(tmpFile.path);

    ASSERT_TRUE(inputFile.isOpen());

    EXPECT_EQ(inputFile.hash(), util::sha256sum(""));

    std::vector<char> buffer(content.size());
    EXPECT_TRUE(inputFile.readRaw(buffer.data(), buffer.size()));
    EXPECT_EQ(std::string(buffer.data(), buffer.size()), content);

    EXPECT_EQ(inputFile.hash(), util::sha256sum(content));
}

TEST_F(InputFileTest, HashProgressesWithReading)
{
    std::string const content = "Hello, World!";
    auto tmpFile = TmpFile{content};
    InputFile inputFile(tmpFile.path);

    ASSERT_TRUE(inputFile.isOpen());

    EXPECT_EQ(inputFile.hash(), util::sha256sum(""));

    // Read first part
    std::vector<char> buffer1(5);
    EXPECT_TRUE(inputFile.readRaw(buffer1.data(), buffer1.size()));
    EXPECT_EQ(inputFile.hash(), util::sha256sum("Hello"));

    // Read second part
    std::vector<char> buffer2(8);
    EXPECT_TRUE(inputFile.readRaw(buffer2.data(), buffer2.size()));
    EXPECT_EQ(inputFile.hash(), util::sha256sum(content));
}
