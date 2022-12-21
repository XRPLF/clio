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

#include <util/Fixtures.h>
#include <util/config/Config.h>

#include <boost/filesystem.hpp>
#include <boost/json/parse.hpp>

#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>

using namespace clio::util;
using namespace boost::log;
using namespace std;
namespace json = boost::json;

constexpr static auto JSONData = R"JSON(
    {
        "arr": [                
            { "first": 1234 },
            { "second": true },
            { "inner_section": [{ "inner": "works" }] },
            ["127.0.0.1", "192.168.0.255"]
        ],
        "section": {
            "test": {
                "str": "hello",
                "int": 9042,
                "bool": true
            }
        },
        "top": 420
    }
)JSON";

class ConfigTest : public NoLoggerFixture
{
protected:
    Config cfg{json::parse(JSONData)};
};

TEST_F(ConfigTest, SanityCheck)
{
    // throw on wrong key format etc.:
    ASSERT_ANY_THROW((void)cfg.value<bool>(""));
    ASSERT_ANY_THROW((void)cfg.value<bool>("a."));
    ASSERT_ANY_THROW((void)cfg.value<bool>(".a"));
    ASSERT_ANY_THROW((void)cfg.valueOr<bool>("", false));
    ASSERT_ANY_THROW((void)cfg.valueOr<bool>("a.", false));
    ASSERT_ANY_THROW((void)cfg.valueOr<bool>(".a", false));
    ASSERT_ANY_THROW((void)cfg.maybeValue<bool>(""));
    ASSERT_ANY_THROW((void)cfg.maybeValue<bool>("a."));
    ASSERT_ANY_THROW((void)cfg.maybeValue<bool>(".a"));
    ASSERT_ANY_THROW((void)cfg.valueOrThrow<bool>("", "custom"));
    ASSERT_ANY_THROW((void)cfg.valueOrThrow<bool>("a.", "custom"));
    ASSERT_ANY_THROW((void)cfg.valueOrThrow<bool>(".a", "custom"));
    ASSERT_ANY_THROW((void)cfg.contains(""));
    ASSERT_ANY_THROW((void)cfg.contains("a."));
    ASSERT_ANY_THROW((void)cfg.contains(".a"));
    ASSERT_ANY_THROW((void)cfg.section(""));
    ASSERT_ANY_THROW((void)cfg.section("a."));
    ASSERT_ANY_THROW((void)cfg.section(".a"));

    // valid path, value does not exists -> optional functions should not throw
    ASSERT_ANY_THROW((void)cfg.value<bool>("b"));
    ASSERT_EQ(cfg.valueOr<bool>("b", false), false);
    ASSERT_EQ(cfg.maybeValue<bool>("b"), std::nullopt);
    ASSERT_ANY_THROW((void)cfg.valueOrThrow<bool>("b", "custom"));
}

TEST_F(ConfigTest, Access)
{
    ASSERT_EQ(cfg.value<int64_t>("top"), 420);
    ASSERT_EQ(cfg.value<string>("section.test.str"), "hello");
    ASSERT_EQ(cfg.value<int64_t>("section.test.int"), 9042);
    ASSERT_EQ(cfg.value<bool>("section.test.bool"), true);

    ASSERT_ANY_THROW((void)cfg.value<uint64_t>(
        "section.test.bool"));  // wrong type requested
    ASSERT_ANY_THROW((void)cfg.value<bool>("section.doesnotexist"));

    ASSERT_EQ(cfg.valueOr<string>("section.test.str", "fallback"), "hello");
    ASSERT_EQ(
        cfg.valueOr<string>("section.test.nonexistent", "fallback"),
        "fallback");
    ASSERT_EQ(cfg.valueOr("section.test.bool", false), true);

    ASSERT_ANY_THROW(
        (void)cfg.valueOr("section.test.bool", 1234));  // wrong type requested
}

TEST_F(ConfigTest, ErrorHandling)
{
    try
    {
        (void)cfg.valueOrThrow<bool>("section.test.int", "msg");
        ASSERT_FALSE(true);  // should not get here
    }
    catch (std::runtime_error const& e)
    {
        ASSERT_STREQ(e.what(), "msg");
    }

    ASSERT_EQ(cfg.valueOrThrow<bool>("section.test.bool", ""), true);

    auto arr = cfg.array("arr");
    try
    {
        (void)arr[3].array()[1].valueOrThrow<int>("msg");  // wrong type
        ASSERT_FALSE(true);  // should not get here
    }
    catch (std::runtime_error const& e)
    {
        ASSERT_STREQ(e.what(), "msg");
    }

    ASSERT_EQ(arr[3].array()[1].valueOrThrow<string>(""), "192.168.0.255");

    try
    {
        (void)cfg.arrayOrThrow("nonexisting.key", "msg");
        ASSERT_FALSE(true);  // should not get here
    }
    catch (std::runtime_error const& e)
    {
        ASSERT_STREQ(e.what(), "msg");
    }

    ASSERT_EQ(cfg.arrayOrThrow("arr", "")[0].value<int>("first"), 1234);
}

TEST_F(ConfigTest, Section)
{
    auto sub = cfg.section("section.test");

    ASSERT_EQ(sub.value<string>("str"), "hello");
    ASSERT_EQ(sub.value<int64_t>("int"), 9042);
    ASSERT_EQ(sub.value<bool>("bool"), true);
}

TEST_F(ConfigTest, Array)
{
    auto arr = cfg.array("arr");

    ASSERT_EQ(arr.size(), 4);
    ASSERT_EQ(arr[0].value<int64_t>("first"), 1234);

    // check twice to verify that previous array(key) access did not destroy the
    // store by using move
    ASSERT_EQ(arr[2].array("inner_section")[0].value<string>("inner"), "works");
    ASSERT_EQ(arr[2].array("inner_section")[0].value<string>("inner"), "works");

    ASSERT_EQ(arr[3].array()[1].value<string>(), "192.168.0.255");

    vector exp{"192.168.0.255", "127.0.0.1"};
    for (auto inner = arr[3].array(); auto const& el : inner)
    {
        ASSERT_EQ(el.value<string>(), exp.back());
        exp.pop_back();
    }

    ASSERT_TRUE(exp.empty());
}

/**
 * @brief Simple custom data type with json parsing support
 */
struct Custom
{
    string a;
    int b;
    bool c;

    friend Custom
    tag_invoke(json::value_to_tag<Custom>, json::value const& value)
    {
        assert(value.is_object());
        auto const& obj = value.as_object();
        return {
            obj.at("str").as_string().c_str(),
            obj.at("int").as_int64(),
            obj.at("bool").as_bool()};
    }
};

TEST_F(ConfigTest, Extend)
{
    auto custom = cfg.value<Custom>("section.test");

    ASSERT_EQ(custom.a, "hello");
    ASSERT_EQ(custom.b, 9042);
    ASSERT_EQ(custom.c, true);
}

/**
 * @brief Simple temporary file util
 */
class TmpFile
{
public:
    TmpFile(std::string const& data)
        : tmpPath_{boost::filesystem::unique_path().string()}
    {
        std::ofstream of;
        of.open(tmpPath_);
        of << data;
        of.close();
    }
    ~TmpFile()
    {
        std::remove(tmpPath_.c_str());
    }
    std::string
    path() const
    {
        return tmpPath_;
    }

private:
    std::string tmpPath_;
};

TEST_F(ConfigTest, File)
{
    auto tmp = TmpFile(JSONData);
    auto conf = ConfigReader::open(tmp.path());

    ASSERT_EQ(conf.value<int64_t>("top"), 420);

    auto doesntexist = ConfigReader::open("nope");
    ASSERT_EQ(doesntexist.valueOr<bool>("found", false), false);
}
