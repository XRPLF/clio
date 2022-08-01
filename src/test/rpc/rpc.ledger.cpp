#include <algorithm>
#include <clio/backend/BackendFactory.h>
#include <clio/backend/DBHelpers.h>
#include <clio/rpc/RPCHelpers.h>
#include <gtest/gtest.h>

#include <test/env/env.h>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

void
writeLedger(
    Backend::BackendInterface& backend,
    boost::asio::yield_context& yield)
{
    std::string rawHeader =
        "03C3141A01633CD656F91B4EBB5EB89B791BD34DBC8A04BB6F407C5335"
        "BC54351E"
        "DD73"
        "3898497E809E04074D14D271E4832D7888754F9230800761563A292FA2"
        "315A6DB6"
        "FE30"
        "CC5909B285080FCD6773CC883F9FE0EE4D439340AC592AADB973ED3CF5"
        "3E2232B3"
        "3EF5"
        "7CECAC2816E3122816E31A0A00F8377CD95DFA484CFAE282656A58CE5A"
        "A29652EF"
        "FD80"
        "AC59CD91416E4E13DBBE";

    auto hexStringToBinaryString = [](auto const& hex) {
        auto blob = ripple::strUnHex(hex);
        std::string strBlob;
        for (auto c : *blob)
        {
            strBlob += c;
        }
        return strBlob;
    };
    auto binaryStringToUint256 = [](auto const& bin) -> ripple::uint256 {
        ripple::uint256 uint;
        return uint.fromVoid((void const*)bin.data());
    };
    auto ledgerInfoToBinaryString = [](auto const& info) {
        auto blob = RPC::ledgerInfoToBlob(info, true);
        std::string strBlob;
        for (auto c : blob)
        {
            strBlob += c;
        }
        return strBlob;
    };

    std::string rawHeaderBlob = hexStringToBinaryString(rawHeader);
    ripple::LedgerInfo lgrInfo =
        deserializeHeader(ripple::makeSlice(rawHeaderBlob));

    backend.startWrites();
    backend.writeLedger(lgrInfo, std::move(rawHeaderBlob));
    ASSERT_TRUE(backend.finishWrites(lgrInfo.seq));
    {
        auto rng = backend.fetchLedgerRange();
        EXPECT_TRUE(rng.has_value());
        EXPECT_EQ(rng->minSequence, rng->maxSequence);
        EXPECT_EQ(rng->maxSequence, lgrInfo.seq);
    }
}

TYPED_TEST_SUITE(Clio, cfgMOCK);

TYPED_TEST(Clio, ledgerIndexNotFound)
{
    boost::asio::io_context ioc;
    std::optional<boost::asio::io_context::work> work;
    work.emplace(ioc);
    std::atomic_bool done = false;

    boost::asio::spawn(
        ioc, [this, &done, &work](boost::asio::yield_context yield) {
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::warning);

            std::string keyspace = this->keyspace();

            auto session = std::make_shared<MockSubscriber>();
            Backend::LedgerRange range;
            range.maxSequence = 63116320;

            writeLedger(this->app().backend(), yield);

            boost::json::object request = {
                {"method", "ledger"}, {"ledger_index", 63116320}};

            auto context = RPC::make_WsContext(
                request, this->app(), session, range, "127.0.0.1", yield);

            ASSERT_TRUE(context);

            auto result = RPC::buildResponse(*context);

            ASSERT_TRUE(std::holds_alternative<RPC::Status>(result));

            ASSERT_EQ(
                std::get<RPC::Status>(result),
                RPC::Status{RPC::Error::rpcLGR_NOT_FOUND});
            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}

TYPED_TEST(Clio, ledgerHashNotFound)
{
    boost::asio::io_context ioc;
    std::optional<boost::asio::io_context::work> work;
    work.emplace(ioc);
    std::atomic_bool done = false;

    boost::asio::spawn(
        ioc, [this, &done, &work](boost::asio::yield_context yield) {
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::warning);

            std::string keyspace = this->keyspace();

            auto session = std::make_shared<MockSubscriber>();
            Backend::LedgerRange range;
            range.minSequence = 1;
            range.maxSequence = 63116320;

            writeLedger(this->app().backend(), yield);

            boost::json::object request = {
                {"method", "ledger"},
                {"ledger_hash",
                 "F8377CD95DFA484CFAE282656A58CE5AA29652EFFD80AC59CD91416E4E13D"
                 "BB4"}};

            auto context = RPC::make_WsContext(
                request, this->app(), session, range, "127.0.0.1", yield);

            ASSERT_TRUE(context);

            auto result = RPC::buildResponse(*context);

            ASSERT_TRUE(std::holds_alternative<RPC::Status>(result));

            ASSERT_EQ(
                std::get<RPC::Status>(result),
                RPC::Status{RPC::Error::rpcLGR_NOT_FOUND});
            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}

TYPED_TEST(Clio, ledgerByHash)
{
    boost::asio::io_context ioc;
    std::optional<boost::asio::io_context::work> work;
    work.emplace(ioc);
    std::atomic_bool done = false;

    boost::asio::spawn(
        ioc, [this, &done, &work](boost::asio::yield_context yield) {
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::warning);

            std::string keyspace = this->keyspace();

            auto session = std::make_shared<MockSubscriber>();
            Backend::LedgerRange range;
            range.minSequence = 1;
            range.maxSequence = 63116320;

            writeLedger(this->app().backend(), yield);

            boost::json::object request = {
                {"method", "ledger"},
                {"ledger_hash",
                 "F8377CD95DFA484CFAE282656A58CE5AA29652EFFD80AC59CD91416E4E13D"
                 "BBE"}};

            auto context = RPC::make_WsContext(
                request, this->app(), session, range, "127.0.0.1", yield);

            ASSERT_TRUE(context);

            auto result = RPC::buildResponse(*context);

            ASSERT_TRUE(std::holds_alternative<boost::json::object>(result));

            auto json = std::get<boost::json::object>(result);

            auto& ledger = json["ledger"].as_object();
            ASSERT_STREQ(
                ledger[JS(ledger_hash)].as_string().c_str(),
                "F8377CD95DFA484CFAE282656A58CE5AA29652EFFD80AC59CD91416E4E13DB"
                "BE");
            ASSERT_STREQ(
                ledger[JS(ledger_index)].as_string().c_str(), "63116314");

            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}

TYPED_TEST(Clio, ledgerByIndex)
{
    boost::asio::io_context ioc;
    std::optional<boost::asio::io_context::work> work;
    work.emplace(ioc);
    std::atomic_bool done = false;

    boost::asio::spawn(
        ioc, [this, &done, &work](boost::asio::yield_context yield) {
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::warning);

            std::string keyspace = this->keyspace();

            auto session = std::make_shared<MockSubscriber>();
            Backend::LedgerRange range;
            range.minSequence = 1;
            range.maxSequence = 63116320;

            writeLedger(this->app().backend(), yield);

            boost::json::object request = {
                {"method", "ledger"}, {"ledger_index", 63116314}};

            auto context = RPC::make_WsContext(
                request, this->app(), session, range, "127.0.0.1", yield);

            ASSERT_TRUE(context);

            auto result = RPC::buildResponse(*context);

            if (!std::holds_alternative<boost::json::object>(result))
            {
                ASSERT_FALSE(true);
                return;
            }

            auto json = std::get<boost::json::object>(result);

            auto& ledger = json[JS(ledger)].as_object();
            ASSERT_STREQ(
                ledger[JS(ledger_hash)].as_string().c_str(),
                "F8377CD95DFA484CFAE282656A58CE5AA29652EFFD80AC59CD91416E4E13DB"
                "BE");
            ASSERT_STREQ(
                ledger[JS(ledger_index)].as_string().c_str(), "63116314");

            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}

TYPED_TEST(Clio, ledgerByValidated)
{
    boost::asio::io_context ioc;
    std::optional<boost::asio::io_context::work> work;
    work.emplace(ioc);
    std::atomic_bool done = false;

    boost::asio::spawn(
        ioc, [this, &done, &work](boost::asio::yield_context yield) {
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::warning);

            std::string keyspace = this->keyspace();

            auto session = std::make_shared<MockSubscriber>();
            Backend::LedgerRange range;
            range.minSequence = 1;
            range.maxSequence = 63116314;

            writeLedger(this->app().backend(), yield);

            boost::json::object request = {
                {"method", "ledger"}, {"ledger_index", "validated"}};

            auto context = RPC::make_WsContext(
                request, this->app(), session, range, "127.0.0.1", yield);

            ASSERT_TRUE(context);

            auto result = RPC::buildResponse(*context);

            if (!std::holds_alternative<boost::json::object>(result))
            {
                ASSERT_FALSE(true);
                return;
            }

            auto json = std::get<boost::json::object>(result);

            auto& ledger = json[JS(ledger)].as_object();
            ASSERT_STREQ(
                ledger[JS(ledger_hash)].as_string().c_str(),
                "F8377CD95DFA484CFAE282656A58CE5AA29652EFFD80AC59CD91416E4E13DB"
                "BE");
            ASSERT_STREQ(
                ledger[JS(ledger_index)].as_string().c_str(), "63116314");

            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}

TYPED_TEST(Clio, ledgerByClosed)
{
    boost::asio::io_context ioc;
    std::optional<boost::asio::io_context::work> work;
    work.emplace(ioc);
    std::atomic_bool done = false;

    boost::asio::spawn(
        ioc, [this, &done, &work](boost::asio::yield_context yield) {
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::warning);

            std::string keyspace = this->keyspace();

            auto session = std::make_shared<MockSubscriber>();
            Backend::LedgerRange range;
            range.minSequence = 1;
            range.maxSequence = 63116314;

            writeLedger(this->app().backend(), yield);

            boost::json::object request = {
                {"method", "ledger"}, {"ledger_index", "closed"}};

            auto context = RPC::make_WsContext(
                request, this->app(), session, range, "127.0.0.1", yield);

            ASSERT_TRUE(context);
            ASSERT_TRUE(RPC::shouldForwardToRippled(*context));

            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}

TYPED_TEST(Clio, ledgerByCurrent)
{
    boost::asio::io_context ioc;
    std::optional<boost::asio::io_context::work> work;
    work.emplace(ioc);
    std::atomic_bool done = false;

    boost::asio::spawn(
        ioc, [this, &done, &work](boost::asio::yield_context yield) {
            boost::log::core::get()->set_filter(
                boost::log::trivial::severity >= boost::log::trivial::warning);

            std::string keyspace = this->keyspace();

            auto session = std::make_shared<MockSubscriber>();
            Backend::LedgerRange range;
            range.minSequence = 1;
            range.maxSequence = 63116314;

            writeLedger(this->app().backend(), yield);

            boost::json::object request = {
                {"method", "ledger"}, {"ledger_index", "current"}};

            auto context = RPC::make_WsContext(
                request, this->app(), session, range, "127.0.0.1", yield);

            ASSERT_TRUE(context);
            ASSERT_TRUE(RPC::shouldForwardToRippled(*context));

            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}
