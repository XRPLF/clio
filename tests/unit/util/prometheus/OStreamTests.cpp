#include "util/prometheus/OStream.hpp"

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <gtest/gtest.h>

#include <string>
#include <utility>

using namespace util::prometheus;

TEST(OStreamTests, empty)
{
    OStream stream{false};
    EXPECT_EQ(std::move(stream).data(), "");
}

TEST(OStreamTests, string)
{
    OStream stream{false};
    stream << "hello";
    EXPECT_EQ(std::move(stream).data(), "hello");
}

TEST(OStreamTests, compression)
{
    OStream stream{true};
    std::string const str = "helloooooooooooooooooooooooooooooooooo";
    stream << str;
    auto const compressed = std::move(stream).data();
    EXPECT_LT(compressed.size(), str.size());

    std::string const decompressed = [&compressed]() {
        std::string result;
        boost::iostreams::filtering_istream stream;
        stream.push(boost::iostreams::gzip_decompressor{});
        stream.push(boost::iostreams::array_source{compressed.data(), compressed.size()});
        stream >> result;
        return result;
    }();
    EXPECT_EQ(decompressed, str);
}
