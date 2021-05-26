#include <gtest/gtest.h>
#include <handlers/RPCHelpers.h>
#include <reporting/BackendFactory.h>
#include <reporting/BackendInterface.h>

// Demonstrate some basic assertions.
TEST(BackendTest, Basic)
{
    std::string keyspace =
        "oceand_test_" +
        std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
    boost::json::object config{
        {"database",
         {{"type", "cassandra"},
          {"cassandra",
           {{"contact_points", "34.222.180.207"},
            {"port", 9042},
            {"keyspace", keyspace.c_str()},
            {"table_prefix", ""},
            {"max_requests_outstanding", 10000},
            {"threads", 8}}}}}};
    auto backend = Backend::makeBackend(config);
    backend->open(false);

    std::string rawHeader =
        "03C3141A01633CD656F91B4EBB5EB89B791BD34DBC8A04BB6F407C5335BC54351EDD73"
        "3898497E809E04074D14D271E4832D7888754F9230800761563A292FA2315A6DB6FE30"
        "CC5909B285080FCD6773CC883F9FE0EE4D439340AC592AADB973ED3CF53E2232B33EF5"
        "7CECAC2816E3122816E31A0A00F8377CD95DFA484CFAE282656A58CE5AA29652EFFD80"
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
    auto ledgerInfoToBinaryString = [](auto const& info) {
        auto blob = ledgerInfoToBlob(info);
        std::string strBlob;
        for (auto c : *blob)
        {
            strBlob += c;
        }
        return strBlob;
    };

    std::string rawHeaderBlob = hexStringToBinaryString(rawHeader);
    ripple::LedgerInfo lgrInfo =
        deserializeHeader(ripple::makeSlice(rawHeaderBlob));

    backend->startWrites();
    backend->writeLedger(lgrInfo, std::move(rawHeaderBlob), true);
    EXPECT_TRUE(backend->finishWrites(lgrInfo.seq));
    {
        auto rng = backend->fetchLedgerRange();
        EXPECT_TRUE(rng.has_value());
        EXPECT_EQ(rng->minSequence, rng->maxSequence);
        EXPECT_EQ(rng->maxSequence, lgrInfo.seq);
    }
    {
        auto seq = backend->fetchLatestLedgerSequence();
        EXPECT_TRUE(seq.has_value());
        EXPECT_EQ(*seq, lgrInfo.seq);
    }

    {
        auto retLgr = backend->fetchLedgerBySequence(lgrInfo.seq);
        EXPECT_TRUE(lgr.has_value());
        EXPECT_EQ(lgr->seq, lgrInfo.seq);
        EXPECT_EQ(ledgerInfoToBlob(lgrInfo), ledgerInfoToBlob(retLgr));
    }

    EXPECT_FALSE(backend->fetchLedgerBySequence(lgrInfo.seq + 1).has_value());
    auto lgrInfoOld = lgrInfo;

    auto lgrInfoNext = lgrInfo;
    lgrInfoNext.seq = lgrInfo.seq + 1;
    lgrInfoNext.accountHash = ~lgrInfo.accountHash;
    {
        std::string rawHeaderBlob = ledgerInfoToBinaryString(lgrInfoNext);

        backend->startWrites();
        backend->writeLedger(lgrInfoNext, std::move(rawHeaderBlob));
        res = backend->finishWrites(lgrInfoNext.seq);
        EXPECT_TRUE(res);
    }
    {
        auto rng = backend->fetchLedgerRange();
        EXPECT_TRUE(rng.has_value());
        EXPECT_EQ(rng->minSequence, lgrInfo.seq - 1);
        EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
    }
    {
        auto seq = backend->fetchLatestLedgerSequence();
        EXPECT_EQ(seq, lgrInfoNext.seq);
    }
    {
        auto retLgr = backend->fetchLedgerBySequence(lgrInfoNext.seq);
        EXPECT_TRUE(lgr.has_value());
        EXPECT_EQ(lgr->seq, lgrInfoNext.seq);
        EXPECT_EQ(ledgerInfoToBlob(*retLgr), ledgerInfoToBlob(lgrInfoNext));
        EXPECT_NE(ledgerInfoToBlob(*retLgr), ledgerInfoToBlob(lgrInfo));
        retLgr = backend->fetchLedgerBySequence(lgrInfoNext.seq - 1);
        EXPECT_EQ(ledgerInfoToBlob(*retLgr), ledgerInfoToBlob(lgrInfo));
        EXPECT_NE(ledgerInfoToBlob(*retLgr), ledgerInfoToBlob(lgrInfoNext));
        retLgr = backend->fetchLedgerBySequence(lgrInfoNext.seq - 2);
        EXPECT_FALSE(
            backend->fetchLedgerBySequence(lgrInfoNext.seq - 2).has_value());
    }

    auto txns = backend->fetchAllTransactionsInLedger(lgrInfoNext.seq);
    EXPECT_EQ(txns.size(), 0);
    auto hashes = backend->fetchAllTransactionHashesInLedger(lgrInfoNext.seq);
    EXPECT_EQ(hashes.size(), 0);

    std::string metadata =
        "201C0000001DF8E311006F5630F58E8E36FD9F77456E6E5B76C8C479D55D2675DC"
        "2B57"
        "8D9EE0FBFD0F4435E7E82400F5ACA25010623C4C4AD65873DA787AC85A0A1385FE"
        "6233"
        "B6DE100799474F19BA75E8F4A44E64D5A0BA986182A59400000000000000000000"
        "0000"
        "434E5900000000000360E3E0751BD9A566CD03FA6CAFC78118B82BA06540000002"
        "F63A"
        "19788114B61B3EB55660F67EAAA4479175D2FDEA71CD940BE1E1E411006456623C"
        "4C4A"
        "D65873DA787AC85A0A1385FE6233B6DE100799474F19B87CAAEB9A59E722000000"
        "0036"
        "4F19B87CAAEB9A5958623C4C4AD65873DA787AC85A0A1385FE6233B6DE10079947"
        "4F19"
        "B87CAAEB9A590111000000000000000000000000434E59000000000002110360E3"
        "E075"
        "1BD9A566CD03FA6CAFC78118B82BA0031100000000000000000000000000000000"
        "0000"
        "000004110000000000000000000000000000000000000000E1E1E311006456623C"
        "4C4A"
        "D65873DA787AC85A0A1385FE6233B6DE100799474F19BA75E8F4A44EE8364F19BA"
        "75E8"
        "F4A44E58623C4C4AD65873DA787AC85A0A1385FE6233B6DE100799474F19BA75E8"
        "F4A4"
        "4E0111000000000000000000000000434E59000000000002110360E3E0751BD9A5"
        "66CD"
        "03FA6CAFC78118B82BA0E1E1E411006F568120731CA1CECDB619E8DAA252098015"
        "8407"
        "F8C587654D5DC8050BE6D5E6F6A4E722000000002400F5AC9E2503CE17F1330000"
        "0000"
        "00000000340000000000000000558614FB8C558DF9DB89BA9D147E6F6540196114"
        "D611"
        "5E4DD3D266DE237D464F5C5010623C4C4AD65873DA787AC85A0A1385FE6233B6DE"
        "1007"
        "99474F19B87CAAEB9A5964D588B6135A6783DB000000000000000000000000434E"
        "5900"
        "000000000360E3E0751BD9A566CD03FA6CAFC78118B82BA06540000000C9DF6DFA"
        "8114"
        "B61B3EB55660F67EAAA4479175D2FDEA71CD940BE1E1E51100612503CE17F95599"
        "5AFC"
        "E2A0B6B925C8BD04158D9AE706518E8CEC1695D78052E412799447C75A56EB0772"
        "83F2"
        "89CE1E0956133D9AD7828C1F88FFE5A50A885AD8679E8AEDBCDAA7E62400F5ACA2"
        "6240"
        "0000012E3449A4E1E722000000002400F5ACA32D0000000562400000012E344998"
        "8114"
        "B61B3EB55660F67EAAA4479175D2FDEA71CD940BE1E1E511006456FE9C40EDE9C0"
        "AE6C"
        "A8023498F9B9092DF3EB722B8B17C0C8A210A2FDCF22C08DE7220000000058FE9C"
        "40ED"
        "E9C0AE6CA8023498F9B9092DF3EB722B8B17C0C8A210A2FDCF22C08D8214B61B3E"
        "B556"
        "60F67EAAA4479175D2FDEA71CD940BE1E1F1031000";
    std::string transaction =
        "12000722000000002400F5ACA2201900F5AC9E201B03CE17FB64D5A0BA986182A5"
        "9400"
        "0000000000000000000000434E5900000000000360E3E0751BD9A566CD03FA6CAF"
        "C781"
        "18B82BA06540000002F63A197868400000000000000C732102EF32A8F811F2D6EA"
        "67FD"
        "BAF625ABE70C0885189AA03A99330B6F7437C88492D0744630440220145530852F"
        "98E6"
        "D2A4D427A045556B6E45E54477BB3BC24952C8DFF3514A0E51022063F6D619D51C"
        "7F60"
        "B64B3CDF1E9EB79F4E7B5E2BDA9C81489CCD93F247F713618114B61B3EB55660F6"
        "7EAA"
        "A4479175D2FDEA71CD940B";

    std::string metaBlob = hexStringToBinaryString(meta);
    std::string txnBlob = hexStringToBinaryString(transaction);
}

