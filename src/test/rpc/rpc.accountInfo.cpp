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
writeAccount(
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
    backend.writeSuccessor(
        uint256ToString(Backend::firstKey),
        lgrInfo.seq,
        uint256ToString(Backend::lastKey));
    ASSERT_TRUE(backend.finishWrites(lgrInfo.seq));
    {
        auto rng = backend.fetchLedgerRange();
        EXPECT_TRUE(rng.has_value());
        EXPECT_EQ(rng->minSequence, rng->maxSequence);
        EXPECT_EQ(rng->maxSequence, lgrInfo.seq);
    }
    {
        auto seq = backend.fetchLatestLedgerSequence(yield);
        EXPECT_TRUE(seq.has_value());
        EXPECT_EQ(*seq, lgrInfo.seq);
    }

    {
        auto retLgr = backend.fetchLedgerBySequence(lgrInfo.seq, yield);
        ASSERT_TRUE(retLgr.has_value());
        EXPECT_EQ(retLgr->seq, lgrInfo.seq);
        EXPECT_EQ(
            RPC::ledgerInfoToBlob(lgrInfo), RPC::ledgerInfoToBlob(*retLgr));
    }

    EXPECT_FALSE(
        backend.fetchLedgerBySequence(lgrInfo.seq + 1, yield).has_value());
    auto lgrInfoOld = lgrInfo;

    auto lgrInfoNext = lgrInfo;
    lgrInfoNext.seq = lgrInfo.seq + 1;
    lgrInfoNext.parentHash = lgrInfo.hash;
    lgrInfoNext.hash++;
    lgrInfoNext.accountHash = ~lgrInfo.accountHash;
    {
        std::string rawHeaderBlob = ledgerInfoToBinaryString(lgrInfoNext);

        backend.startWrites();
        backend.writeLedger(lgrInfoNext, std::move(rawHeaderBlob));
        ASSERT_TRUE(backend.finishWrites(lgrInfoNext.seq));
    }
    {
        auto rng = backend.fetchLedgerRange();
        EXPECT_TRUE(rng.has_value());
        EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
        EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
    }
    {
        auto seq = backend.fetchLatestLedgerSequence(yield);
        EXPECT_EQ(seq, lgrInfoNext.seq);
    }
    {
        auto retLgr = backend.fetchLedgerBySequence(lgrInfoNext.seq, yield);
        EXPECT_TRUE(retLgr.has_value());
        EXPECT_EQ(retLgr->seq, lgrInfoNext.seq);
        EXPECT_EQ(
            RPC::ledgerInfoToBlob(*retLgr), RPC::ledgerInfoToBlob(lgrInfoNext));
        EXPECT_NE(
            RPC::ledgerInfoToBlob(*retLgr), RPC::ledgerInfoToBlob(lgrInfoOld));
        retLgr = backend.fetchLedgerBySequence(lgrInfoNext.seq - 1, yield);
        EXPECT_EQ(
            RPC::ledgerInfoToBlob(*retLgr), RPC::ledgerInfoToBlob(lgrInfoOld));
        EXPECT_NE(
            RPC::ledgerInfoToBlob(*retLgr), RPC::ledgerInfoToBlob(lgrInfoNext));
        retLgr = backend.fetchLedgerBySequence(lgrInfoNext.seq - 2, yield);
        EXPECT_FALSE(backend.fetchLedgerBySequence(lgrInfoNext.seq - 2, yield)
                         .has_value());

        auto txns =
            backend.fetchAllTransactionsInLedger(lgrInfoNext.seq, yield);
        EXPECT_EQ(txns.size(), 0);

        auto hashes =
            backend.fetchAllTransactionHashesInLedger(lgrInfoNext.seq, yield);
        EXPECT_EQ(hashes.size(), 0);
    }

    // the below dummy data is not expected to be consistent. The
    // metadata string does represent valid metadata. Don't assume
    // though that the transaction or its hash correspond to the
    // metadata, or anything like that. These tests are purely
    // binary tests to make sure the same data that goes in, comes
    // back out
    std::string metaHex =
        "201C0000001AF8E411006F560A3E08122A05AC91DEFA87052B0554E4A2"
        "9B46"
        "3A27642EBB060B6052196592EEE72200000000240480FDB52503CE1A86"
        "3300"
        "000000000000003400000000000000005529983CBAED30F54747145292"
        "1C3C"
        "6B9F9685F292F6291000EED0A44413AF18C250101AC09600F4B502C8F7"
        "F830"
        "F80B616DCB6F3970CB79AB70975A05ED5B66860B9564400000001FE217"
        "CB65"
        "D54B640B31521B05000000000000000000000000434E59000000000003"
        "60E3"
        "E0751BD9A566CD03FA6CAFC78118B82BA081142252F328CF9126341776"
        "2570"
        "D67220CCB33B1370E1E1E3110064561AC09600F4B502C8F7F830F80B61"
        "6DCB"
        "6F3970CB79AB70975A05ED33DF783681E8365A05ED33DF783681581AC0"
        "9600"
        "F4B502C8F7F830F80B616DCB6F3970CB79AB70975A05ED33DF78368103"
        "1100"
        "0000000000000000000000434E59000000000004110360E3E0751BD9A5"
        "66CD"
        "03FA6CAFC78118B82BA0E1E1E4110064561AC09600F4B502C8F7F830F8"
        "0B61"
        "6DCB6F3970CB79AB70975A05ED5B66860B95E72200000000365A05ED5B"
        "6686"
        "0B95581AC09600F4B502C8F7F830F80B616DCB6F3970CB79AB70975A05"
        "ED5B"
        "66860B9501110000000000000000000000000000000000000000021100"
        "0000"
        "0000000000000000000000000000000000031100000000000000000000"
        "0000"
        "434E59000000000004110360E3E0751BD9A566CD03FA6CAFC78118B82B"
        "A0E1"
        "E1E311006F5647B05E66DE9F3DF2689E8F4CE6126D3136B6C5E79587F9"
        "D24B"
        "D71A952B0852BAE8240480FDB950101AC09600F4B502C8F7F830F80B61"
        "6DCB"
        "6F3970CB79AB70975A05ED33DF78368164400000033C83A95F65D59D9A"
        "6291"
        "9C2D18000000000000000000000000434E5900000000000360E3E0751B"
        "D9A5"
        "66CD03FA6CAFC78118B82BA081142252F328CF91263417762570D67220"
        "CCB3"
        "3B1370E1E1E511006456AEA3074F10FE15DAC592F8A0405C61FB7D4C98"
        "F588"
        "C2D55C84718FAFBBD2604AE72200000000310000000000000000320000"
        "0000"
        "0000000058AEA3074F10FE15DAC592F8A0405C61FB7D4C98F588C2D55C"
        "8471"
        "8FAFBBD2604A82142252F328CF91263417762570D67220CCB33B1370E1"
        "E1E5"
        "1100612503CE1A8755CE935137F8C6C8DEF26B5CD93BE18105CA83F65E"
        "1E90"
        "CEC546F562D25957DC0856E0311EB450B6177F969B94DBDDA83E99B7A0"
        "576A"
        "CD9079573876F16C0C004F06E6240480FDB9624000000005FF0E2BE1E7"
        "2200"
        "000000240480FDBA2D00000005624000000005FF0E1F81142252F328CF"
        "9126"
        "3417762570D67220CCB33B1370E1E1F1031000";
    std::string txnHex =
        "1200072200000000240480FDB920190480FDB5201B03CE1A8964400000"
        "033C"
        "83A95F65D59D9A62919C2D18000000000000000000000000434E590000"
        "0000"
        "000360E3E0751BD9A566CD03FA6CAFC78118B82BA06840000000000000"
        "0C73"
        "21022D40673B44C82DEE1DDB8B9BB53DCCE4F97B27404DB850F068DD91"
        "D685"
        "E337EA7446304402202EA6B702B48B39F2197112382838F92D4C02948E"
        "9911"
        "FE6B2DEBCF9183A426BC022005DAC06CD4517E86C2548A80996019F3AC"
        "60A0"
        "9EED153BF60C992930D68F09F981142252F328CF91263417762570D672"
        "20CC"
        "B33B1370";
    std::string hashHex =
        "0A81FB3D6324C2DCF73131505C6E4DC67981D7FC39F5E9574CEC4B1F22"
        "D28BF7";

    // this account is not related to the above transaction and
    // metadata
    std::string accountHex =
        "1100612200000000240480FDBC2503CE1A872D0000000555516931B2AD"
        "018EFFBE"
        "17C5"
        "C9DCCF872F36837C2C6136ACF80F2A24079CF81FD0624000000005FF0E"
        "07811422"
        "52F3"
        "28CF91263417762570D67220CCB33B1370";
    std::string accountIndexHex =
        "E0311EB450B6177F969B94DBDDA83E99B7A0576ACD9079573876F16C0C"
        "004F06";

    std::string metaBlob = hexStringToBinaryString(metaHex);
    std::string txnBlob = hexStringToBinaryString(txnHex);
    std::string hashBlob = hexStringToBinaryString(hashHex);
    std::string accountBlob = hexStringToBinaryString(accountHex);
    std::string accountIndexBlob = hexStringToBinaryString(accountIndexHex);

    ripple::SLE sle(
        ripple::SerialIter(accountBlob.data(), accountBlob.size()),
        ripple::uint256::fromVoid(accountIndexBlob.data()));

    std::vector<ripple::AccountID> affectedAccounts;

    {
        backend.startWrites();
        lgrInfoNext.seq = lgrInfoNext.seq + 1;
        lgrInfoNext.txHash = ~lgrInfo.txHash;
        lgrInfoNext.accountHash = lgrInfoNext.accountHash ^ lgrInfoNext.txHash;
        lgrInfoNext.parentHash = lgrInfoNext.hash;
        lgrInfoNext.hash++;

        ripple::uint256 hash256;
        EXPECT_TRUE(hash256.parseHex(hashHex));
        ripple::TxMeta txMeta{hash256, lgrInfoNext.seq, metaBlob};
        auto accountsSet = txMeta.getAffectedAccounts();

        std::vector<AccountTransactionsData> accountTxData;
        accountTxData.emplace_back(txMeta, hash256);
        backend.writeLedger(lgrInfoNext, ledgerInfoToBinaryString(lgrInfoNext));
        backend.writeTransaction(
            std::string{hashBlob},
            lgrInfoNext.seq,
            lgrInfoNext.closeTime.time_since_epoch().count(),
            std::string{txnBlob},
            std::string{metaBlob});
        backend.writeAccountTransactions(std::move(accountTxData));
        backend.writeLedgerObject(
            std::string{accountIndexBlob},
            lgrInfoNext.seq,
            std::string{accountBlob});
        backend.writeSuccessor(
            uint256ToString(Backend::firstKey),
            lgrInfoNext.seq,
            std::string{accountIndexBlob});
        backend.writeSuccessor(
            std::string{accountIndexBlob},
            lgrInfoNext.seq,
            uint256ToString(Backend::lastKey));

        ASSERT_TRUE(backend.finishWrites(lgrInfoNext.seq));
    }
}

TYPED_TEST_SUITE(Clio, cfgMOCK);

TYPED_TEST(Clio, accountInfo)
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
            range.maxSequence = 63116316;

            writeAccount(this->app().backend(), yield);

            boost::json::object request = {
                {"method", "account_info"},
                {"account", "rh3VLyj1GbQjX7eA15BwUagEhSrPHmLkSR"},
                {"ledger_index", 63116316}};

            auto context = RPC::make_WsContext(
                request, this->app(), session, range, "127.0.0.1", yield);

            ASSERT_TRUE(context);

            auto result = RPC::buildResponse(*context);

            ASSERT_TRUE(std::holds_alternative<boost::json::object>(result));

            auto obj = std::get<boost::json::object>(result);

            ASSERT_TRUE(obj[JS(validated)].as_bool());
            ASSERT_EQ(obj[JS(ledger_index)].as_uint64(), 63116316);
            auto data = obj[JS(account_data)].as_object();
            ASSERT_EQ(
                data[JS(Account)].as_string(),
                "rh3VLyj1GbQjX7eA15BwUagEhSrPHmLkSR");
            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}

TYPED_TEST(Clio, accountInfoNotFound)
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

            writeAccount(this->app().backend(), yield);

            boost::json::object request = {
                {"method", "account_info"},
                {"account", "rh3VLyj1GbQjX7eA15BwUagEhSrPHmLkSR"},
                {"ledger_index", 63116310}};

            auto context = RPC::make_WsContext(
                request, this->app(), session, range, "127.0.0.1", yield);

            ASSERT_TRUE(context);

            auto result = RPC::buildResponse(*context);

            ASSERT_TRUE(std::holds_alternative<RPC::Status>(result));

            ASSERT_EQ(
                std::get<RPC::Status>(result),
                RPC::Status{RPC::Error::rpcACT_NOT_FOUND});
            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}

TYPED_TEST(Clio, accountInfoMalformed)
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

            writeAccount(this->app().backend(), yield);

            boost::json::object request = {
                {"method", "account_info"},
                {"account", "rh3VLyj1GbQjX7eA15BwUagEhSrPHmLkS"},
                {"ledger_index", 63116316}};

            auto context = RPC::make_WsContext(
                request, this->app(), session, range, "127.0.0.1", yield);

            ASSERT_TRUE(context);

            auto result = RPC::buildResponse(*context);

            ASSERT_TRUE(std::holds_alternative<RPC::Status>(result));

            ASSERT_EQ(
                std::get<RPC::Status>(result),
                RPC::Status{RPC::Error::rpcACT_MALFORMED});

            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}
