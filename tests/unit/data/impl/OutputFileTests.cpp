#include "data/impl/OutputFile.hpp"
#include "util/Shasum.hpp"
#include "util/TmpFile.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <numbers>
#include <string>
#include <vector>

using namespace data::impl;

struct OutputFileTest : ::testing::Test {
    TmpFile tmpFile = TmpFile::empty();

    [[nodiscard]] std::string
    readFileContents() const
    {
        std::ifstream ifs(tmpFile.path, std::ios::binary);
        return std::string{std::istreambuf_iterator<char>{ifs}, std::istreambuf_iterator<char>{}};
    }
};

TEST_F(OutputFileTest, ConstructorOpensFile)
{
    OutputFile const file(tmpFile.path);
    EXPECT_TRUE(file.isOpen());
}

TEST_F(OutputFileTest, NonExistingFile)
{
    std::string const invalidPath = "/invalid/nonexistent/directory/file.dat";
    OutputFile const file(invalidPath);
    EXPECT_FALSE(file.isOpen());
}

TEST_F(OutputFileTest, WriteBasicTypes)
{
    uint32_t const intValue = 0x12345678;
    double const doubleValue = std::numbers::pi;
    char const charValue = 'A';
    {
        OutputFile file(tmpFile.path);

        file.write(intValue);
        file.write(doubleValue);
        file.write(charValue);
    }

    std::string contents = readFileContents();
    EXPECT_EQ(contents.size(), sizeof(intValue) + sizeof(doubleValue) + sizeof(charValue));

    auto* data = reinterpret_cast<char const*>(contents.data());
    EXPECT_EQ(*reinterpret_cast<uint32_t const*>(data), intValue);
    EXPECT_EQ(*reinterpret_cast<double const*>(data + sizeof(intValue)), doubleValue);
    EXPECT_EQ(*(data + sizeof(intValue) + sizeof(doubleValue)), charValue);
}

TEST_F(OutputFileTest, WriteArray)
{
    std::vector<uint32_t> const data = {0x11111111, 0x22222222, 0x33333333, 0x44444444};

    {
        OutputFile file(tmpFile.path);
        file.write(data.data(), data.size() * sizeof(uint32_t));
    }

    std::string contents = readFileContents();
    EXPECT_EQ(contents.size(), data.size() * sizeof(uint32_t));

    auto* readData = reinterpret_cast<uint32_t const*>(contents.data());
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(readData[i], data[i]);
    }
}

TEST_F(OutputFileTest, WriteRawData)
{
    std::string const testData = "Hello, World!";
    {
        OutputFile file(tmpFile.path);
        file.writeRaw(testData.data(), testData.size());
    }

    std::string const contents = readFileContents();
    EXPECT_EQ(contents, testData);
}

TEST_F(OutputFileTest, WriteMultipleChunks)
{
    std::string chunk1 = "First chunk";
    std::string chunk2 = "Second chunk";
    std::string chunk3 = "Third chunk";

    {
        OutputFile file(tmpFile.path);

        file.writeRaw(chunk1.data(), chunk1.size());
        file.writeRaw(chunk2.data(), chunk2.size());
        file.writeRaw(chunk3.data(), chunk3.size());
    }

    std::string const contents = readFileContents();
    EXPECT_EQ(contents, chunk1 + chunk2 + chunk3);
}

TEST_F(OutputFileTest, HashOfEmptyFile)
{
    OutputFile const file(tmpFile.path);
    ASSERT_TRUE(file.isOpen());

    // Hash of empty file should match SHA256 of empty string
    EXPECT_EQ(file.hash(), util::sha256sum(""));
}

TEST_F(OutputFileTest, HashAfterWriting)
{
    std::string const testData = "Hello, World!";
    {
        OutputFile file(tmpFile.path);
        file.writeRaw(testData.data(), testData.size());

        // Hash should match SHA256 of the written data
        EXPECT_EQ(file.hash(), util::sha256sum(testData));
    }
}

TEST_F(OutputFileTest, HashProgressesWithWrites)
{
    std::string const part1 = "Hello, ";
    std::string const part2 = "World!";
    std::string const combined = part1 + part2;

    OutputFile file(tmpFile.path);
    ASSERT_TRUE(file.isOpen());

    EXPECT_EQ(file.hash(), util::sha256sum(""));

    file.writeRaw(part1.data(), part1.size());
    EXPECT_EQ(file.hash(), util::sha256sum(part1));

    file.writeRaw(part2.data(), part2.size());
    EXPECT_EQ(file.hash(), util::sha256sum(combined));
}
