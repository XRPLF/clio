//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "util/prometheus/OStream.h"
#include <string>

#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <gtest/gtest.h>
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
