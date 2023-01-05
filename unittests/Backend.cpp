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

#include <data/BackendInterface.h>
#include <data/DBHelpers.h>
#include <data/Factories.h>
#include <etl/ReportingETL.h>
#include <rpc/RPCHelpers.h>
#include <util/Fixtures.h>
#include <util/config/Config.h>
#include <util/log/Logger.h>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <gtest/gtest.h>

#include <algorithm>

using namespace clio;
using namespace clio::data;
using namespace clio::etl;
using namespace clio::rpc;

class BackendTest : public NoLoggerFixture
{
};

TEST_F(BackendTest, Basic)
{
    boost::asio::io_context ioc;
    std::optional<boost::asio::io_context::work> work;
    work.emplace(ioc);
    std::atomic_bool done = false;

    boost::asio::spawn(
        ioc, [&done, &work, &ioc](boost::asio::yield_context yield) {
            boost::log::core::get()->set_filter(
                util::log_severity >= util::Severity::WRN);
            std::string keyspace = "clio_test_" +
                std::to_string(std::chrono::system_clock::now()
                                   .time_since_epoch()
                                   .count());
            boost::json::object cassandraConfig{
                {"database",
                 {{"type", "cassandra"},
                  {"cassandra",
                   {{"contact_points", "127.0.0.1"},
                    {"port", 9042},
                    {"keyspace", keyspace.c_str()},
                    {"replication_factor", 1},
                    {"table_prefix", ""},
                    {"max_requests_outstanding", 1000},
                    {"indexer_key_shift", 2},
                    {"threads", 8}}}}}};
            std::vector<boost::json::object> configs = {cassandraConfig};
            for (auto& config : configs)
            {
                auto backend = make_Backend(ioc, util::Config{config});

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
                auto binaryStringToUint256 =
                    [](auto const& bin) -> ripple::uint256 {
                    ripple::uint256 uint;
                    return uint.fromVoid((void const*)bin.data());
                };
                auto ledgerInfoToBinaryString = [](auto const& info) {
                    auto blob = rpc::ledgerInfoToBlob(info, true);
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

                backend->startWrites();
                backend->writeLedger(lgrInfo, std::move(rawHeaderBlob));
                backend->writeSuccessor(
                    uint256ToString(data::firstKey),
                    lgrInfo.seq,
                    uint256ToString(data::lastKey));
                ASSERT_TRUE(backend->finishWrites(lgrInfo.seq));
                {
                    auto rng = backend->fetchLedgerRange();
                    EXPECT_TRUE(rng.has_value());
                    EXPECT_EQ(rng->minSequence, rng->maxSequence);
                    EXPECT_EQ(rng->maxSequence, lgrInfo.seq);
                }
                {
                    auto seq = backend->fetchLatestLedgerSequence(yield);
                    EXPECT_TRUE(seq.has_value());
                    EXPECT_EQ(*seq, lgrInfo.seq);
                }

                {
                    auto retLgr =
                        backend->fetchLedgerBySequence(lgrInfo.seq, yield);
                    ASSERT_TRUE(retLgr.has_value());
                    EXPECT_EQ(retLgr->seq, lgrInfo.seq);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(lgrInfo),
                        rpc::ledgerInfoToBlob(*retLgr));
                }

                EXPECT_FALSE(
                    backend->fetchLedgerBySequence(lgrInfo.seq + 1, yield)
                        .has_value());
                auto lgrInfoOld = lgrInfo;

                auto lgrInfoNext = lgrInfo;
                lgrInfoNext.seq = lgrInfo.seq + 1;
                lgrInfoNext.parentHash = lgrInfo.hash;
                lgrInfoNext.hash++;
                lgrInfoNext.accountHash = ~lgrInfo.accountHash;
                {
                    std::string rawHeaderBlob =
                        ledgerInfoToBinaryString(lgrInfoNext);

                    backend->startWrites();
                    backend->writeLedger(lgrInfoNext, std::move(rawHeaderBlob));
                    ASSERT_TRUE(backend->finishWrites(lgrInfoNext.seq));
                }
                {
                    auto rng = backend->fetchLedgerRange();
                    EXPECT_TRUE(rng.has_value());
                    EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
                    EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
                }
                {
                    auto seq = backend->fetchLatestLedgerSequence(yield);
                    EXPECT_EQ(seq, lgrInfoNext.seq);
                }
                {
                    auto retLgr =
                        backend->fetchLedgerBySequence(lgrInfoNext.seq, yield);
                    EXPECT_TRUE(retLgr.has_value());
                    EXPECT_EQ(retLgr->seq, lgrInfoNext.seq);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfoNext));
                    EXPECT_NE(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfoOld));
                    retLgr = backend->fetchLedgerBySequence(
                        lgrInfoNext.seq - 1, yield);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfoOld));
                    EXPECT_NE(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfoNext));
                    retLgr = backend->fetchLedgerBySequence(
                        lgrInfoNext.seq - 2, yield);
                    EXPECT_FALSE(
                        backend
                            ->fetchLedgerBySequence(lgrInfoNext.seq - 2, yield)
                            .has_value());

                    auto txns = backend->fetchAllTransactionsInLedger(
                        lgrInfoNext.seq, yield);
                    EXPECT_EQ(txns.size(), 0);

                    auto hashes = backend->fetchAllTransactionHashesInLedger(
                        lgrInfoNext.seq, yield);
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

                // An NFTokenMint tx
                std::string nftTxnHex =
                    "1200192200000008240011CC9B201B001F71D6202A0000000168400000"
                    "000000000C7321ED475D1452031E8F9641AF1631519A58F7B8681E172E"
                    "4838AA0E59408ADA1727DD74406960041F34F10E0CBB39444B4D4E577F"
                    "C0B7E8D843D091C2917E96E7EE0E08B30C91413EC551A2B8A1D405E8BA"
                    "34FE185D8B10C53B40928611F2DE3B746F0303751868747470733A2F2F"
                    "677265677765697362726F642E636F6D81146203F49C21D5D6E022CB16"
                    "DE3538F248662FC73C";

                std::string nftTxnMeta =
                    "201C00000001F8E511005025001F71B3556ED9C9459001E4F4A9121F4E"
                    "07AB6D14898A5BBEF13D85C25D743540DB59F3CF566203F49C21D5D6E0"
                    "22CB16DE3538F248662FC73CFFFFFFFFFFFFFFFFFFFFFFFFE6FAEC5A00"
                    "0800006203F49C21D5D6E022CB16DE3538F248662FC73C8962EFA00000"
                    "0006751868747470733A2F2F677265677765697362726F642E636F6DE1"
                    "EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73C93E8B1"
                    "C200000028751868747470733A2F2F677265677765697362726F642E63"
                    "6F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73C"
                    "9808B6B90000001D751868747470733A2F2F677265677765697362726F"
                    "642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F24866"
                    "2FC73C9C28BBAC00000012751868747470733A2F2F6772656777656973"
                    "62726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538"
                    "F248662FC73CA048C0A300000007751868747470733A2F2F6772656777"
                    "65697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16"
                    "DE3538F248662FC73CAACE82C500000029751868747470733A2F2F6772"
                    "65677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6E0"
                    "22CB16DE3538F248662FC73CAEEE87B80000001E751868747470733A2F"
                    "2F677265677765697362726F642E636F6DE1EC5A000800006203F49C21"
                    "D5D6E022CB16DE3538F248662FC73CB30E8CAF00000013751868747470"
                    "733A2F2F677265677765697362726F642E636F6DE1EC5A000800006203"
                    "F49C21D5D6E022CB16DE3538F248662FC73CB72E91A200000008751868"
                    "747470733A2F2F677265677765697362726F642E636F6DE1EC5A000800"
                    "006203F49C21D5D6E022CB16DE3538F248662FC73CC1B453C40000002A"
                    "751868747470733A2F2F677265677765697362726F642E636F6DE1EC5A"
                    "000800006203F49C21D5D6E022CB16DE3538F248662FC73CC5D458BB00"
                    "00001F751868747470733A2F2F677265677765697362726F642E636F6D"
                    "E1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73CC9F4"
                    "5DAE00000014751868747470733A2F2F677265677765697362726F642E"
                    "636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC7"
                    "3CCE1462A500000009751868747470733A2F2F67726567776569736272"
                    "6F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248"
                    "662FC73CD89A24C70000002B751868747470733A2F2F67726567776569"
                    "7362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE35"
                    "38F248662FC73CDCBA29BA00000020751868747470733A2F2F67726567"
                    "7765697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB"
                    "16DE3538F248662FC73CE0DA2EB100000015751868747470733A2F2F67"
                    "7265677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6"
                    "E022CB16DE3538F248662FC73CE4FA33A40000000A751868747470733A"
                    "2F2F677265677765697362726F642E636F6DE1EC5A000800006203F49C"
                    "21D5D6E022CB16DE3538F248662FC73CF39FFABD000000217518687474"
                    "70733A2F2F677265677765697362726F642E636F6DE1EC5A0008000062"
                    "03F49C21D5D6E022CB16DE3538F248662FC73CF7BFFFB0000000167518"
                    "68747470733A2F2F677265677765697362726F642E636F6DE1EC5A0008"
                    "00006203F49C21D5D6E022CB16DE3538F248662FC73CFBE004A7000000"
                    "0B751868747470733A2F2F677265677765697362726F642E636F6DE1F1"
                    "E1E72200000000501A6203F49C21D5D6E022CB16DE3538F248662FC73C"
                    "662FC73C8962EFA000000006FAEC5A000800006203F49C21D5D6E022CB"
                    "16DE3538F248662FC73C8962EFA000000006751868747470733A2F2F67"
                    "7265677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6"
                    "E022CB16DE3538F248662FC73C93E8B1C200000028751868747470733A"
                    "2F2F677265677765697362726F642E636F6DE1EC5A000800006203F49C"
                    "21D5D6E022CB16DE3538F248662FC73C9808B6B90000001D7518687474"
                    "70733A2F2F677265677765697362726F642E636F6DE1EC5A0008000062"
                    "03F49C21D5D6E022CB16DE3538F248662FC73C9C28BBAC000000127518"
                    "68747470733A2F2F677265677765697362726F642E636F6DE1EC5A0008"
                    "00006203F49C21D5D6E022CB16DE3538F248662FC73CA048C0A3000000"
                    "07751868747470733A2F2F677265677765697362726F642E636F6DE1EC"
                    "5A000800006203F49C21D5D6E022CB16DE3538F248662FC73CAACE82C5"
                    "00000029751868747470733A2F2F677265677765697362726F642E636F"
                    "6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73CAE"
                    "EE87B80000001E751868747470733A2F2F677265677765697362726F64"
                    "2E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662F"
                    "C73CB30E8CAF00000013751868747470733A2F2F677265677765697362"
                    "726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F2"
                    "48662FC73CB72E91A200000008751868747470733A2F2F677265677765"
                    "697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE"
                    "3538F248662FC73CC1B453C40000002A751868747470733A2F2F677265"
                    "677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022"
                    "CB16DE3538F248662FC73CC5D458BB0000001F751868747470733A2F2F"
                    "677265677765697362726F642E636F6DE1EC5A000800006203F49C21D5"
                    "D6E022CB16DE3538F248662FC73CC9F45DAE0000001475186874747073"
                    "3A2F2F677265677765697362726F642E636F6DE1EC5A000800006203F4"
                    "9C21D5D6E022CB16DE3538F248662FC73CCE1462A50000000975186874"
                    "7470733A2F2F677265677765697362726F642E636F6DE1EC5A00080000"
                    "6203F49C21D5D6E022CB16DE3538F248662FC73CD89A24C70000002B75"
                    "1868747470733A2F2F677265677765697362726F642E636F6DE1EC5A00"
                    "0800006203F49C21D5D6E022CB16DE3538F248662FC73CDCBA29BA0000"
                    "0020751868747470733A2F2F677265677765697362726F642E636F6DE1"
                    "EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73CE0DA2E"
                    "B100000015751868747470733A2F2F677265677765697362726F642E63"
                    "6F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F248662FC73C"
                    "E4FA33A40000000A751868747470733A2F2F677265677765697362726F"
                    "642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538F24866"
                    "2FC73CEF7FF5C60000002C751868747470733A2F2F6772656777656973"
                    "62726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16DE3538"
                    "F248662FC73CF39FFABD00000021751868747470733A2F2F6772656777"
                    "65697362726F642E636F6DE1EC5A000800006203F49C21D5D6E022CB16"
                    "DE3538F248662FC73CF7BFFFB000000016751868747470733A2F2F6772"
                    "65677765697362726F642E636F6DE1EC5A000800006203F49C21D5D6E0"
                    "22CB16DE3538F248662FC73CFBE004A70000000B751868747470733A2F"
                    "2F677265677765697362726F642E636F6DE1F1E1E1E511006125001F71"
                    "B3556ED9C9459001E4F4A9121F4E07AB6D14898A5BBEF13D85C25D7435"
                    "40DB59F3CF56BE121B82D5812149D633F605EB07265A80B762A365CE94"
                    "883089FEEE4B955701E6240011CC9B202B0000002C6240000002540BE3"
                    "ECE1E72200000000240011CC9C2D0000000A202B0000002D202C000000"
                    "066240000002540BE3E081146203F49C21D5D6E022CB16DE3538F24866"
                    "2FC73CE1E1F1031000";
                std::string nftTxnHashHex =
                    "6C7F69A6D25A13AC4A2E9145999F45D4674F939900017A96885FDC2757"
                    "E9284E";
                ripple::uint256 nftID;
                EXPECT_TRUE(
                    nftID.parseHex("000800006203F49C21D5D6E022CB16DE3538F248662"
                                   "FC73CEF7FF5C60000002C"));

                std::string metaBlob = hexStringToBinaryString(metaHex);
                std::string txnBlob = hexStringToBinaryString(txnHex);
                std::string hashBlob = hexStringToBinaryString(hashHex);
                std::string accountBlob = hexStringToBinaryString(accountHex);
                std::string accountIndexBlob =
                    hexStringToBinaryString(accountIndexHex);
                std::vector<ripple::AccountID> affectedAccounts;

                std::string nftTxnBlob = hexStringToBinaryString(nftTxnHex);
                std::string nftTxnMetaBlob =
                    hexStringToBinaryString(nftTxnMeta);

                {
                    backend->startWrites();
                    lgrInfoNext.seq = lgrInfoNext.seq + 1;
                    lgrInfoNext.txHash = ~lgrInfo.txHash;
                    lgrInfoNext.accountHash =
                        lgrInfoNext.accountHash ^ lgrInfoNext.txHash;
                    lgrInfoNext.parentHash = lgrInfoNext.hash;
                    lgrInfoNext.hash++;

                    ripple::uint256 hash256;
                    EXPECT_TRUE(hash256.parseHex(hashHex));
                    ripple::TxMeta txMeta{hash256, lgrInfoNext.seq, metaBlob};
                    auto journal = ripple::debugLog();
                    auto accountsSet = txMeta.getAffectedAccounts();
                    for (auto& a : accountsSet)
                    {
                        affectedAccounts.push_back(a);
                    }
                    std::vector<AccountTransactionsData> accountTxData;
                    accountTxData.emplace_back(txMeta, hash256, journal);

                    ripple::uint256 nftHash256;
                    EXPECT_TRUE(nftHash256.parseHex(nftTxnHashHex));
                    ripple::TxMeta nftTxMeta{
                        nftHash256, lgrInfoNext.seq, nftTxnMetaBlob};
                    ripple::SerialIter it{nftTxnBlob.data(), nftTxnBlob.size()};
                    ripple::STTx sttx{it};
                    auto const [parsedNFTTxsRef, parsedNFT] =
                        getNFTData(nftTxMeta, sttx);
                    // need to copy the nft txns so we can std::move later
                    std::vector<NFTTransactionsData> parsedNFTTxs;
                    parsedNFTTxs.insert(
                        parsedNFTTxs.end(),
                        parsedNFTTxsRef.begin(),
                        parsedNFTTxsRef.end());
                    EXPECT_EQ(parsedNFTTxs.size(), 1);
                    EXPECT_TRUE(parsedNFT.has_value());
                    EXPECT_EQ(parsedNFT->tokenID, nftID);
                    std::vector<NFTsData> nftData;
                    nftData.push_back(*parsedNFT);

                    backend->writeLedger(
                        lgrInfoNext, ledgerInfoToBinaryString(lgrInfoNext));
                    backend->writeTransaction(
                        std::string{hashBlob},
                        lgrInfoNext.seq,
                        lgrInfoNext.closeTime.time_since_epoch().count(),
                        std::string{txnBlob},
                        std::string{metaBlob});
                    backend->writeAccountTransactions(std::move(accountTxData));

                    // NFT writing not yet implemented for pg
                    if (config == cassandraConfig)
                    {
                        backend->writeNFTs(std::move(nftData));
                        backend->writeNFTTransactions(std::move(parsedNFTTxs));
                    }
                    else
                    {
                        EXPECT_THROW(
                            { backend->writeNFTs(std::move(nftData)); },
                            std::runtime_error);
                        EXPECT_THROW(
                            {
                                backend->writeNFTTransactions(
                                    std::move(parsedNFTTxs));
                            },
                            std::runtime_error);
                    }

                    backend->writeLedgerObject(
                        std::string{accountIndexBlob},
                        lgrInfoNext.seq,
                        std::string{accountBlob});
                    backend->writeSuccessor(
                        uint256ToString(data::firstKey),
                        lgrInfoNext.seq,
                        std::string{accountIndexBlob});
                    backend->writeSuccessor(
                        std::string{accountIndexBlob},
                        lgrInfoNext.seq,
                        uint256ToString(data::lastKey));

                    ASSERT_TRUE(backend->finishWrites(lgrInfoNext.seq));
                }

                {
                    auto rng = backend->fetchLedgerRange();
                    EXPECT_TRUE(rng);
                    EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
                    EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
                    auto retLgr =
                        backend->fetchLedgerBySequence(lgrInfoNext.seq, yield);
                    EXPECT_TRUE(retLgr);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfoNext));
                    auto txns = backend->fetchAllTransactionsInLedger(
                        lgrInfoNext.seq, yield);
                    EXPECT_EQ(txns.size(), 1);
                    EXPECT_STREQ(
                        (const char*)txns[0].transaction.data(),
                        (const char*)txnBlob.data());
                    EXPECT_STREQ(
                        (const char*)txns[0].metadata.data(),
                        (const char*)metaBlob.data());
                    auto hashes = backend->fetchAllTransactionHashesInLedger(
                        lgrInfoNext.seq, yield);
                    EXPECT_EQ(hashes.size(), 1);
                    EXPECT_EQ(ripple::strHex(hashes[0]), hashHex);
                    for (auto& a : affectedAccounts)
                    {
                        auto [txns, cursor] = backend->fetchAccountTransactions(
                            a, 100, true, {}, yield);
                        EXPECT_EQ(txns.size(), 1);
                        EXPECT_EQ(txns[0], txns[0]);
                        EXPECT_FALSE(cursor);
                    }

                    // NFT fetching not yet implemented for pg
                    if (config == cassandraConfig)
                    {
                        auto nft =
                            backend->fetchNFT(nftID, lgrInfoNext.seq, yield);
                        EXPECT_TRUE(nft.has_value());
                        auto [nftTxns, cursor] = backend->fetchNFTTransactions(
                            nftID, 100, true, {}, yield);
                        EXPECT_EQ(nftTxns.size(), 1);
                        EXPECT_EQ(nftTxns[0], nftTxns[0]);
                        EXPECT_FALSE(cursor);
                    }
                    else
                    {
                        EXPECT_THROW(
                            {
                                backend->fetchNFT(
                                    nftID, lgrInfoNext.seq, yield);
                            },
                            std::runtime_error);
                        EXPECT_THROW(
                            {
                                backend->fetchNFTTransactions(
                                    nftID, 100, true, {}, yield);
                            },
                            std::runtime_error);
                    }

                    ripple::uint256 key256;
                    EXPECT_TRUE(key256.parseHex(accountIndexHex));
                    auto obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq, yield);
                    EXPECT_TRUE(obj);
                    EXPECT_STREQ(
                        (const char*)obj->data(),
                        (const char*)accountBlob.data());
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq + 1, yield);
                    EXPECT_TRUE(obj);
                    EXPECT_STREQ(
                        (const char*)obj->data(),
                        (const char*)accountBlob.data());
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoOld.seq - 1, yield);
                    EXPECT_FALSE(obj);
                }
                // obtain a time-based seed:
                unsigned seed =
                    std::chrono::system_clock::now().time_since_epoch().count();
                std::string accountBlobOld = accountBlob;
                {
                    backend->startWrites();
                    lgrInfoNext.seq = lgrInfoNext.seq + 1;
                    lgrInfoNext.parentHash = lgrInfoNext.hash;
                    lgrInfoNext.hash++;
                    lgrInfoNext.txHash =
                        lgrInfoNext.txHash ^ lgrInfoNext.accountHash;
                    lgrInfoNext.accountHash =
                        ~(lgrInfoNext.accountHash ^ lgrInfoNext.txHash);

                    backend->writeLedger(
                        lgrInfoNext, ledgerInfoToBinaryString(lgrInfoNext));
                    std::shuffle(
                        accountBlob.begin(),
                        accountBlob.end(),
                        std::default_random_engine(seed));
                    backend->writeLedgerObject(
                        std::string{accountIndexBlob},
                        lgrInfoNext.seq,
                        std::string{accountBlob});

                    ASSERT_TRUE(backend->finishWrites(lgrInfoNext.seq));
                }
                {
                    auto rng = backend->fetchLedgerRange();
                    EXPECT_TRUE(rng);
                    EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
                    EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
                    auto retLgr =
                        backend->fetchLedgerBySequence(lgrInfoNext.seq, yield);
                    EXPECT_TRUE(retLgr);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfoNext));
                    auto txns = backend->fetchAllTransactionsInLedger(
                        lgrInfoNext.seq, yield);
                    EXPECT_EQ(txns.size(), 0);

                    ripple::uint256 key256;
                    EXPECT_TRUE(key256.parseHex(accountIndexHex));
                    auto obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq, yield);
                    EXPECT_TRUE(obj);
                    EXPECT_STREQ(
                        (const char*)obj->data(),
                        (const char*)accountBlob.data());
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq + 1, yield);
                    EXPECT_TRUE(obj);
                    EXPECT_STREQ(
                        (const char*)obj->data(),
                        (const char*)accountBlob.data());
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq - 1, yield);
                    EXPECT_TRUE(obj);
                    EXPECT_STREQ(
                        (const char*)obj->data(),
                        (const char*)accountBlobOld.data());
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoOld.seq - 1, yield);
                    EXPECT_FALSE(obj);
                }
                {
                    backend->startWrites();
                    lgrInfoNext.seq = lgrInfoNext.seq + 1;
                    lgrInfoNext.parentHash = lgrInfoNext.hash;
                    lgrInfoNext.hash++;
                    lgrInfoNext.txHash =
                        lgrInfoNext.txHash ^ lgrInfoNext.accountHash;
                    lgrInfoNext.accountHash =
                        ~(lgrInfoNext.accountHash ^ lgrInfoNext.txHash);

                    backend->writeLedger(
                        lgrInfoNext, ledgerInfoToBinaryString(lgrInfoNext));
                    backend->writeLedgerObject(
                        std::string{accountIndexBlob},
                        lgrInfoNext.seq,
                        std::string{});
                    backend->writeSuccessor(
                        uint256ToString(data::firstKey),
                        lgrInfoNext.seq,
                        uint256ToString(data::lastKey));

                    ASSERT_TRUE(backend->finishWrites(lgrInfoNext.seq));
                }
                {
                    auto rng = backend->fetchLedgerRange();
                    EXPECT_TRUE(rng);
                    EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
                    EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
                    auto retLgr =
                        backend->fetchLedgerBySequence(lgrInfoNext.seq, yield);
                    EXPECT_TRUE(retLgr);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfoNext));
                    auto txns = backend->fetchAllTransactionsInLedger(
                        lgrInfoNext.seq, yield);
                    EXPECT_EQ(txns.size(), 0);

                    ripple::uint256 key256;
                    EXPECT_TRUE(key256.parseHex(accountIndexHex));
                    auto obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq, yield);
                    EXPECT_FALSE(obj);
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq + 1, yield);
                    EXPECT_FALSE(obj);
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq - 2, yield);
                    EXPECT_TRUE(obj);
                    EXPECT_STREQ(
                        (const char*)obj->data(),
                        (const char*)accountBlobOld.data());
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoOld.seq - 1, yield);
                    EXPECT_FALSE(obj);
                }

                auto generateObjects = [](size_t numObjects,
                                          uint32_t ledgerSequence) {
                    std::vector<std::pair<std::string, std::string>> res{
                        numObjects};
                    ripple::uint256 key;
                    key = ledgerSequence * 100000;

                    for (auto& blob : res)
                    {
                        ++key;
                        std::string keyStr{(const char*)key.data(), key.size()};
                        blob.first = keyStr;
                        blob.second = std::to_string(ledgerSequence) + keyStr;
                    }
                    return res;
                };
                auto updateObjects = [](uint32_t ledgerSequence, auto objs) {
                    for (auto& [key, obj] : objs)
                    {
                        obj = std::to_string(ledgerSequence) + obj;
                    }
                    return objs;
                };
                auto generateTxns = [](size_t numTxns,
                                       uint32_t ledgerSequence) {
                    std::vector<
                        std::tuple<std::string, std::string, std::string>>
                        res{numTxns};
                    ripple::uint256 base;
                    base = ledgerSequence * 100000;
                    for (auto& blob : res)
                    {
                        ++base;
                        std::string hashStr{
                            (const char*)base.data(), base.size()};
                        std::string txnStr =
                            "tx" + std::to_string(ledgerSequence) + hashStr;
                        std::string metaStr =
                            "meta" + std::to_string(ledgerSequence) + hashStr;
                        blob = std::make_tuple(hashStr, txnStr, metaStr);
                    }
                    return res;
                };
                auto generateAccounts = [](uint32_t ledgerSequence,
                                           uint32_t numAccounts) {
                    std::vector<ripple::AccountID> accounts;
                    ripple::AccountID base;
                    base = ledgerSequence * 998765;
                    for (size_t i = 0; i < numAccounts; ++i)
                    {
                        ++base;
                        accounts.push_back(base);
                    }
                    return accounts;
                };
                auto generateAccountTx = [&](uint32_t ledgerSequence,
                                             auto txns) {
                    std::vector<AccountTransactionsData> ret;
                    auto accounts = generateAccounts(ledgerSequence, 10);
                    std::srand(std::time(nullptr));
                    uint32_t idx = 0;
                    for (auto& [hash, txn, meta] : txns)
                    {
                        AccountTransactionsData data;
                        data.ledgerSequence = ledgerSequence;
                        data.transactionIndex = idx;
                        data.txHash = hash;
                        for (size_t i = 0; i < 3; ++i)
                        {
                            data.accounts.insert(
                                accounts[std::rand() % accounts.size()]);
                        }
                        ++idx;
                        ret.push_back(data);
                    }
                    return ret;
                };

                auto generateNextLedger = [seed](auto lgrInfo) {
                    ++lgrInfo.seq;
                    lgrInfo.parentHash = lgrInfo.hash;
                    std::srand(std::time(nullptr));
                    std::shuffle(
                        lgrInfo.txHash.begin(),
                        lgrInfo.txHash.end(),
                        std::default_random_engine(seed));
                    std::shuffle(
                        lgrInfo.accountHash.begin(),
                        lgrInfo.accountHash.end(),
                        std::default_random_engine(seed));
                    std::shuffle(
                        lgrInfo.hash.begin(),
                        lgrInfo.hash.end(),
                        std::default_random_engine(seed));
                    return lgrInfo;
                };
                auto writeLedger = [&](auto lgrInfo,
                                       auto txns,
                                       auto objs,
                                       auto accountTx,
                                       auto state) {
                    backend->startWrites();

                    backend->writeLedger(
                        lgrInfo, ledgerInfoToBinaryString(lgrInfo));
                    for (auto [hash, txn, meta] : txns)
                    {
                        backend->writeTransaction(
                            std::move(hash),
                            lgrInfo.seq,
                            lgrInfo.closeTime.time_since_epoch().count(),
                            std::move(txn),
                            std::move(meta));
                    }
                    for (auto [key, obj] : objs)
                    {
                        backend->writeLedgerObject(
                            std::string{key}, lgrInfo.seq, std::string{obj});
                    }
                    if (state.count(lgrInfo.seq - 1) == 0 ||
                        std::find_if(
                            state[lgrInfo.seq - 1].begin(),
                            state[lgrInfo.seq - 1].end(),
                            [&](auto obj) {
                                return obj.first == objs[0].first;
                            }) == state[lgrInfo.seq - 1].end())
                    {
                        for (size_t i = 0; i < objs.size(); ++i)
                        {
                            if (i + 1 < objs.size())
                                backend->writeSuccessor(
                                    std::string{objs[i].first},
                                    lgrInfo.seq,
                                    std::string{objs[i + 1].first});
                            else
                                backend->writeSuccessor(
                                    std::string{objs[i].first},
                                    lgrInfo.seq,
                                    uint256ToString(data::lastKey));
                        }
                        if (state.count(lgrInfo.seq - 1))
                            backend->writeSuccessor(
                                std::string{
                                    state[lgrInfo.seq - 1].back().first},
                                lgrInfo.seq,
                                std::string{objs[0].first});
                        else
                            backend->writeSuccessor(
                                uint256ToString(data::firstKey),
                                lgrInfo.seq,
                                std::string{objs[0].first});
                    }

                    backend->writeAccountTransactions(std::move(accountTx));

                    ASSERT_TRUE(backend->finishWrites(lgrInfo.seq));
                };

                auto checkLedger = [&](auto lgrInfo,
                                       auto txns,
                                       auto objs,
                                       auto accountTx) {
                    auto rng = backend->fetchLedgerRange();
                    auto seq = lgrInfo.seq;
                    EXPECT_TRUE(rng);
                    EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
                    EXPECT_GE(rng->maxSequence, seq);
                    auto retLgr = backend->fetchLedgerBySequence(seq, yield);
                    EXPECT_TRUE(retLgr);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfo));
                    // retLgr = backend->fetchLedgerByHash(lgrInfo.hash);
                    // EXPECT_TRUE(retLgr);
                    // EXPECT_EQ(rpc::ledgerInfoToBlob(*retLgr),
                    // rpc::ledgerInfoToBlob(lgrInfo));
                    auto retTxns =
                        backend->fetchAllTransactionsInLedger(seq, yield);
                    for (auto [hash, txn, meta] : txns)
                    {
                        bool found = false;
                        for (auto [retTxn, retMeta, retSeq, retDate] : retTxns)
                        {
                            if (std::strncmp(
                                    (const char*)retTxn.data(),
                                    (const char*)txn.data(),
                                    txn.size()) == 0 &&
                                std::strncmp(
                                    (const char*)retMeta.data(),
                                    (const char*)meta.data(),
                                    meta.size()) == 0)
                                found = true;
                        }
                        ASSERT_TRUE(found);
                    }
                    for (auto [account, data] : accountTx)
                    {
                        std::vector<data::TransactionAndMetadata> retData;
                        std::optional<data::TransactionsCursor> cursor;
                        do
                        {
                            uint32_t limit = 10;
                            auto [txns, retCursor] =
                                backend->fetchAccountTransactions(
                                    account, limit, false, cursor, yield);
                            if (retCursor)
                                EXPECT_EQ(txns.size(), limit);
                            retData.insert(
                                retData.end(), txns.begin(), txns.end());
                            cursor = retCursor;
                        } while (cursor);
                        EXPECT_EQ(retData.size(), data.size());
                        for (size_t i = 0; i < retData.size(); ++i)
                        {
                            auto [txn, meta, seq, date] = retData[i];
                            auto [hash, expTxn, expMeta] = data[i];
                            EXPECT_STREQ(
                                (const char*)txn.data(),
                                (const char*)expTxn.data());
                            EXPECT_STREQ(
                                (const char*)meta.data(),
                                (const char*)expMeta.data());
                        }
                    }
                    std::vector<ripple::uint256> keys;
                    for (auto [key, obj] : objs)
                    {
                        auto retObj = backend->fetchLedgerObject(
                            binaryStringToUint256(key), seq, yield);
                        if (obj.size())
                        {
                            ASSERT_TRUE(retObj.has_value());
                            EXPECT_STREQ(
                                (const char*)obj.data(),
                                (const char*)retObj->data());
                        }
                        else
                        {
                            ASSERT_FALSE(retObj.has_value());
                        }
                        keys.push_back(binaryStringToUint256(key));
                    }

                    {
                        auto retObjs =
                            backend->fetchLedgerObjects(keys, seq, yield);
                        ASSERT_EQ(retObjs.size(), objs.size());

                        for (size_t i = 0; i < keys.size(); ++i)
                        {
                            auto [key, obj] = objs[i];
                            auto retObj = retObjs[i];
                            if (obj.size())
                            {
                                ASSERT_TRUE(retObj.size());
                                EXPECT_STREQ(
                                    (const char*)obj.data(),
                                    (const char*)retObj.data());
                            }
                            else
                            {
                                ASSERT_FALSE(retObj.size());
                            }
                        }
                    }

                    data::LedgerPage page;
                    std::vector<data::LedgerObject> retObjs;
                    size_t numLoops = 0;
                    do
                    {
                        uint32_t limit = 10;
                        page = backend->fetchLedgerPage(
                            page.cursor, seq, limit, false, yield);
                        // if (page.cursor)
                        //    EXPECT_EQ(page.objects.size(), limit);
                        retObjs.insert(
                            retObjs.end(),
                            page.objects.begin(),
                            page.objects.end());
                        ++numLoops;
                    } while (page.cursor);

                    for (auto obj : objs)
                    {
                        bool found = false;
                        for (auto retObj : retObjs)
                        {
                            if (ripple::strHex(obj.first) ==
                                ripple::strHex(retObj.key))
                            {
                                found = true;
                                ASSERT_EQ(
                                    ripple::strHex(obj.second),
                                    ripple::strHex(retObj.blob));
                            }
                        }
                        if (found != (obj.second.size() != 0))
                            ASSERT_EQ(found, obj.second.size() != 0);
                    }
                };

                std::map<
                    uint32_t,
                    std::vector<std::pair<std::string, std::string>>>
                    state;
                std::map<
                    uint32_t,
                    std::vector<
                        std::tuple<std::string, std::string, std::string>>>
                    allTxns;
                std::unordered_map<
                    std::string,
                    std::pair<std::string, std::string>>
                    allTxnsMap;
                std::map<
                    uint32_t,
                    std::map<ripple::AccountID, std::vector<std::string>>>
                    allAccountTx;
                std::map<uint32_t, ripple::LedgerInfo> lgrInfos;
                for (size_t i = 0; i < 10; ++i)
                {
                    lgrInfoNext = generateNextLedger(lgrInfoNext);
                    auto objs = generateObjects(25, lgrInfoNext.seq);
                    auto txns = generateTxns(10, lgrInfoNext.seq);
                    auto accountTx = generateAccountTx(lgrInfoNext.seq, txns);
                    for (auto rec : accountTx)
                    {
                        for (auto account : rec.accounts)
                        {
                            allAccountTx[lgrInfoNext.seq][account].push_back(
                                std::string{
                                    (const char*)rec.txHash.data(),
                                    rec.txHash.size()});
                        }
                    }
                    EXPECT_EQ(objs.size(), 25);
                    EXPECT_NE(objs[0], objs[1]);
                    EXPECT_EQ(txns.size(), 10);
                    EXPECT_NE(txns[0], txns[1]);
                    std::sort(objs.begin(), objs.end());
                    state[lgrInfoNext.seq] = objs;
                    writeLedger(lgrInfoNext, txns, objs, accountTx, state);
                    allTxns[lgrInfoNext.seq] = txns;
                    lgrInfos[lgrInfoNext.seq] = lgrInfoNext;
                    for (auto& [hash, txn, meta] : txns)
                    {
                        allTxnsMap[hash] = std::make_pair(txn, meta);
                    }
                }

                std::vector<std::pair<std::string, std::string>> objs;
                for (size_t i = 0; i < 10; ++i)
                {
                    lgrInfoNext = generateNextLedger(lgrInfoNext);
                    if (!objs.size())
                        objs = generateObjects(25, lgrInfoNext.seq);
                    else
                        objs = updateObjects(lgrInfoNext.seq, objs);
                    auto txns = generateTxns(10, lgrInfoNext.seq);
                    auto accountTx = generateAccountTx(lgrInfoNext.seq, txns);
                    for (auto rec : accountTx)
                    {
                        for (auto account : rec.accounts)
                        {
                            allAccountTx[lgrInfoNext.seq][account].push_back(
                                std::string{
                                    (const char*)rec.txHash.data(),
                                    rec.txHash.size()});
                        }
                    }
                    EXPECT_EQ(objs.size(), 25);
                    EXPECT_NE(objs[0], objs[1]);
                    EXPECT_EQ(txns.size(), 10);
                    EXPECT_NE(txns[0], txns[1]);
                    std::sort(objs.begin(), objs.end());
                    state[lgrInfoNext.seq] = objs;
                    writeLedger(lgrInfoNext, txns, objs, accountTx, state);
                    allTxns[lgrInfoNext.seq] = txns;
                    lgrInfos[lgrInfoNext.seq] = lgrInfoNext;
                    for (auto& [hash, txn, meta] : txns)
                    {
                        allTxnsMap[hash] = std::make_pair(txn, meta);
                    }
                }

                auto flatten = [&](uint32_t max) {
                    std::vector<std::pair<std::string, std::string>> flat;
                    std::map<std::string, std::string> objs;
                    for (auto [seq, diff] : state)
                    {
                        for (auto [k, v] : diff)
                        {
                            if (seq > max)
                            {
                                if (objs.count(k) == 0)
                                    objs[k] = "";
                            }
                            else
                            {
                                objs[k] = v;
                            }
                        }
                    }
                    for (auto [key, value] : objs)
                    {
                        flat.push_back(std::make_pair(key, value));
                    }
                    return flat;
                };

                auto flattenAccountTx = [&](uint32_t max) {
                    std::unordered_map<
                        ripple::AccountID,
                        std::vector<
                            std::tuple<std::string, std::string, std::string>>>
                        accountTx;
                    for (auto [seq, map] : allAccountTx)
                    {
                        if (seq > max)
                            break;
                        for (auto& [account, hashes] : map)
                        {
                            for (auto& hash : hashes)
                            {
                                auto& [txn, meta] = allTxnsMap[hash];
                                accountTx[account].push_back(
                                    std::make_tuple(hash, txn, meta));
                            }
                        }
                    }
                    for (auto& [account, data] : accountTx)
                        std::reverse(data.begin(), data.end());
                    return accountTx;
                };

                for (auto [seq, diff] : state)
                {
                    auto flat = flatten(seq);
                    checkLedger(
                        lgrInfos[seq],
                        allTxns[seq],
                        flat,
                        flattenAccountTx(seq));
                }
            }

            done = true;
            work.reset();
        });

    ioc.run();
    EXPECT_EQ(done, true);
}

TEST_F(BackendTest, cache)
{
    boost::log::core::get()->set_filter(
        util::log_severity >= util::Severity::WRN);
    SimpleCache cache;
    ASSERT_FALSE(cache.isFull());
    cache.setFull();

    // Nothing in cache
    {
        ASSERT_TRUE(cache.isFull());
        ASSERT_EQ(cache.size(), 0);
        ASSERT_FALSE(cache.get(ripple::uint256{12}, 0));
        ASSERT_FALSE(cache.getSuccessor(firstKey, 0));
        ASSERT_FALSE(cache.getPredecessor(lastKey, 0));
    }

    // insert
    uint32_t curSeq = 1;
    std::vector<LedgerObject> objs;
    objs.push_back({});
    objs[0] = {ripple::uint256{42}, {0xCC}};
    cache.update(objs, curSeq);
    {
        auto& obj = objs[0];
        ASSERT_TRUE(cache.isFull());
        ASSERT_EQ(cache.size(), 1);
        auto cacheObj = cache.get(obj.key, curSeq);
        ASSERT_TRUE(cacheObj);
        ASSERT_EQ(*cacheObj, obj.blob);
        ASSERT_FALSE(cache.get(obj.key, curSeq + 1));
        ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
        ASSERT_FALSE(cache.getSuccessor(obj.key, curSeq));
        ASSERT_FALSE(cache.getPredecessor(obj.key, curSeq));
        auto succ = cache.getSuccessor(firstKey, curSeq);
        ASSERT_TRUE(succ);
        ASSERT_EQ(*succ, obj);
        auto pred = cache.getPredecessor(lastKey, curSeq);
        ASSERT_TRUE(pred);
        ASSERT_EQ(pred, obj);
    }
    // update
    curSeq++;
    objs[0].blob = {0x01};
    cache.update(objs, curSeq);
    {
        auto& obj = objs[0];
        ASSERT_EQ(cache.size(), 1);
        auto cacheObj = cache.get(obj.key, curSeq);
        ASSERT_TRUE(cacheObj);
        ASSERT_EQ(*cacheObj, obj.blob);
        ASSERT_FALSE(cache.get(obj.key, curSeq + 1));
        ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
        ASSERT_TRUE(cache.isFull());
        ASSERT_FALSE(cache.getSuccessor(obj.key, curSeq));
        ASSERT_FALSE(cache.getPredecessor(obj.key, curSeq));
        auto succ = cache.getSuccessor(firstKey, curSeq);
        ASSERT_TRUE(succ);
        ASSERT_EQ(*succ, obj);
        auto pred = cache.getPredecessor(lastKey, curSeq);
        ASSERT_TRUE(pred);
        ASSERT_EQ(*pred, obj);
    }
    // empty update
    curSeq++;
    cache.update({}, curSeq);
    {
        auto& obj = objs[0];
        ASSERT_EQ(cache.size(), 1);
        auto cacheObj = cache.get(obj.key, curSeq);
        ASSERT_TRUE(cacheObj);
        ASSERT_EQ(*cacheObj, obj.blob);
        ASSERT_TRUE(cache.get(obj.key, curSeq - 1));
        ASSERT_FALSE(cache.get(obj.key, curSeq - 2));
        ASSERT_EQ(*cache.get(obj.key, curSeq - 1), obj.blob);
        ASSERT_FALSE(cache.getSuccessor(obj.key, curSeq));
        ASSERT_FALSE(cache.getPredecessor(obj.key, curSeq));
        auto succ = cache.getSuccessor(firstKey, curSeq);
        ASSERT_TRUE(succ);
        ASSERT_EQ(*succ, obj);
        auto pred = cache.getPredecessor(lastKey, curSeq);
        ASSERT_TRUE(pred);
        ASSERT_EQ(*pred, obj);
    }
    // delete
    curSeq++;
    objs[0].blob = {};
    cache.update(objs, curSeq);
    {
        auto& obj = objs[0];
        ASSERT_EQ(cache.size(), 0);
        auto cacheObj = cache.get(obj.key, curSeq);
        ASSERT_FALSE(cacheObj);
        ASSERT_FALSE(cache.get(obj.key, curSeq + 1));
        ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
        ASSERT_TRUE(cache.isFull());
        ASSERT_FALSE(cache.getSuccessor(obj.key, curSeq));
        ASSERT_FALSE(cache.getPredecessor(obj.key, curSeq));
        ASSERT_FALSE(cache.getSuccessor(firstKey, curSeq));
        ASSERT_FALSE(cache.getPredecessor(lastKey, curSeq));
    }
    // random non-existent object
    {
        ASSERT_FALSE(cache.get(ripple::uint256{23}, curSeq));
    }

    // insert several objects
    curSeq++;
    objs.resize(10);
    for (size_t i = 0; i < objs.size(); ++i)
    {
        objs[i] = {
            ripple::uint256{i * 100 + 1},
            {(unsigned char)i, (unsigned char)i * 2, (unsigned char)i + 1}};
    }
    cache.update(objs, curSeq);
    {
        ASSERT_EQ(cache.size(), 10);
        for (auto& obj : objs)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
            ASSERT_FALSE(cache.get(obj.key, curSeq + 1));
        }

        std::optional<LedgerObject> succ = {{firstKey, {}}};
        size_t idx = 0;
        while ((succ = cache.getSuccessor(succ->key, curSeq)))
        {
            ASSERT_EQ(*succ, objs[idx++]);
        }
        ASSERT_EQ(idx, objs.size());
    }

    // insert several more objects
    curSeq++;
    auto objs2 = objs;
    for (size_t i = 0; i < objs.size(); ++i)
    {
        objs2[i] = {
            ripple::uint256{i * 100 + 50},
            {(unsigned char)i, (unsigned char)i * 3, (unsigned char)i + 5}};
    }
    cache.update(objs2, curSeq);
    {
        ASSERT_EQ(cache.size(), 20);
        for (auto& obj : objs)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            cacheObj = cache.get(obj.key, curSeq - 1);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, curSeq - 2));
            ASSERT_FALSE(cache.get(obj.key, curSeq + 1));
        }
        for (auto& obj : objs2)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
            ASSERT_FALSE(cache.get(obj.key, curSeq + 1));
        }
        std::optional<LedgerObject> succ = {{firstKey, {}}};
        size_t idx = 0;
        while ((succ = cache.getSuccessor(succ->key, curSeq)))
        {
            if (idx % 2 == 0)
                ASSERT_EQ(*succ, objs[(idx++) / 2]);
            else
                ASSERT_EQ(*succ, objs2[(idx++) / 2]);
        }
        ASSERT_EQ(idx, objs.size() + objs2.size());
    }

    // mix of inserts, updates and deletes
    curSeq++;
    for (size_t i = 0; i < objs.size(); ++i)
    {
        if (i % 2 == 0)
            objs[i].blob = {};
        else if (i % 2 == 1)
            std::reverse(objs[i].blob.begin(), objs[i].blob.end());
    }
    cache.update(objs, curSeq);
    {
        ASSERT_EQ(cache.size(), 15);
        for (size_t i = 0; i < objs.size(); ++i)
        {
            auto& obj = objs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            if (i % 2 == 0)
            {
                ASSERT_FALSE(cacheObj);
                ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
                ASSERT_FALSE(cache.get(obj.key, curSeq - 2));
            }
            else
            {
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
                ASSERT_FALSE(cache.get(obj.key, curSeq - 1));
                ASSERT_FALSE(cache.get(obj.key, curSeq - 2));
            }
        }
        for (auto& obj : objs2)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            cacheObj = cache.get(obj.key, curSeq - 1);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, curSeq - 2));
        }

        auto allObjs = objs;
        allObjs.clear();
        std::copy_if(
            objs.begin(),
            objs.end(),
            std::back_inserter(allObjs),
            [](auto obj) { return obj.blob.size() > 0; });
        std::copy(objs2.begin(), objs2.end(), std::back_inserter(allObjs));
        std::sort(allObjs.begin(), allObjs.end(), [](auto a, auto b) {
            return a.key < b.key;
        });
        std::optional<LedgerObject> succ = {{firstKey, {}}};
        size_t idx = 0;
        while ((succ = cache.getSuccessor(succ->key, curSeq)))
        {
            ASSERT_EQ(*succ, allObjs[idx++]);
        }
        ASSERT_EQ(idx, allObjs.size());
    }
}

TEST_F(BackendTest, cacheBackground)
{
    boost::log::core::get()->set_filter(
        util::log_severity >= util::Severity::WRN);
    SimpleCache cache;
    ASSERT_FALSE(cache.isFull());
    ASSERT_EQ(cache.size(), 0);

    uint32_t startSeq = 10;
    uint32_t curSeq = startSeq;

    std::vector<LedgerObject> bObjs;
    bObjs.resize(100);
    for (size_t i = 0; i < bObjs.size(); ++i)
    {
        bObjs[i].key = ripple::uint256{i * 3 + 1};
        bObjs[i].blob = {(unsigned char)i + 1};
    }
    {
        auto objs = bObjs;
        objs.clear();
        std::copy(bObjs.begin(), bObjs.begin() + 10, std::back_inserter(objs));
        cache.update(objs, startSeq);
        ASSERT_EQ(cache.size(), 10);
        ASSERT_FALSE(cache.isFull());
        for (auto& obj : objs)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
        }
    }
    // some updates
    curSeq++;
    std::vector<LedgerObject> objs1;
    for (size_t i = 0; i < bObjs.size(); ++i)
    {
        if (i % 5 == 0)
            objs1.push_back(bObjs[i]);
    }
    for (auto& obj : objs1)
    {
        std::reverse(obj.blob.begin(), obj.blob.end());
    }
    cache.update(objs1, curSeq);

    {
        for (auto& obj : objs1)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (size_t i = 0; i < 10; i++)
        {
            auto& obj = bObjs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
                return o.key == obj.key;
            });
            if (newObj == objs1.end())
            {
                ASSERT_EQ(*cacheObj, obj.blob);
                cacheObj = cache.get(obj.key, startSeq);
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
            }
            else
            {
                ASSERT_EQ(*cacheObj, newObj->blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
        }
    }

    {
        auto objs = bObjs;
        objs.clear();
        std::copy(
            bObjs.begin() + 10, bObjs.begin() + 20, std::back_inserter(objs));
        cache.update(objs, startSeq, true);
    }
    {
        for (auto& obj : objs1)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (size_t i = 0; i < 20; i++)
        {
            auto& obj = bObjs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
                return o.key == obj.key;
            });
            if (newObj == objs1.end())
            {
                ASSERT_EQ(*cacheObj, obj.blob);
                cacheObj = cache.get(obj.key, startSeq);
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
            }
            else
            {
                ASSERT_EQ(*cacheObj, newObj->blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
        }
    }

    // some inserts
    curSeq++;
    auto objs2 = objs1;
    objs2.clear();
    for (size_t i = 0; i < bObjs.size(); ++i)
    {
        if (i % 7 == 0)
        {
            auto obj = bObjs[i];
            obj.key = ripple::uint256{(i + 1) * 1000};
            obj.blob = {(unsigned char)(i + 1) * 100};
            objs2.push_back(obj);
        }
    }
    cache.update(objs2, curSeq);
    {
        for (auto& obj : objs1)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (auto& obj : objs2)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (size_t i = 0; i < 20; i++)
        {
            auto& obj = bObjs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
                return o.key == obj.key;
            });
            if (newObj == objs1.end())
            {
                ASSERT_EQ(*cacheObj, obj.blob);
                cacheObj = cache.get(obj.key, startSeq);
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
            }
            else
            {
                ASSERT_EQ(*cacheObj, newObj->blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
        }
    }

    {
        auto objs = bObjs;
        objs.clear();
        std::copy(
            bObjs.begin() + 20, bObjs.begin() + 30, std::back_inserter(objs));
        cache.update(objs, startSeq, true);
    }
    {
        for (auto& obj : objs1)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (auto& obj : objs2)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (size_t i = 0; i < 30; i++)
        {
            auto& obj = bObjs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
                return o.key == obj.key;
            });
            if (newObj == objs1.end())
            {
                ASSERT_EQ(*cacheObj, obj.blob);
                cacheObj = cache.get(obj.key, startSeq);
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
            }
            else
            {
                ASSERT_EQ(*cacheObj, newObj->blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
        }
    }

    // some deletes
    curSeq++;
    auto objs3 = objs1;
    objs3.clear();
    for (size_t i = 0; i < bObjs.size(); ++i)
    {
        if (i % 6 == 0)
        {
            auto obj = bObjs[i];
            obj.blob = {};
            objs3.push_back(obj);
        }
    }
    cache.update(objs3, curSeq);
    {
        for (auto& obj : objs1)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            if (std::find_if(objs3.begin(), objs3.end(), [&](auto o) {
                    return o.key == obj.key;
                }) == objs3.end())
            {
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
            else
            {
                ASSERT_FALSE(cacheObj);
            }
        }
        for (auto& obj : objs2)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (auto& obj : objs3)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_FALSE(cacheObj);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (size_t i = 0; i < 30; i++)
        {
            auto& obj = bObjs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
                return o.key == obj.key;
            });
            auto delObj = std::find_if(objs3.begin(), objs3.end(), [&](auto o) {
                return o.key == obj.key;
            });
            if (delObj != objs3.end())
            {
                ASSERT_FALSE(cacheObj);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
            else if (newObj == objs1.end())
            {
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
                cacheObj = cache.get(obj.key, startSeq);
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
            }
            else
            {
                ASSERT_EQ(*cacheObj, newObj->blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
        }
    }
    {
        auto objs = bObjs;
        objs.clear();
        std::copy(bObjs.begin() + 30, bObjs.end(), std::back_inserter(objs));
        cache.update(objs, startSeq, true);
    }
    {
        for (auto& obj : objs1)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            if (std::find_if(objs3.begin(), objs3.end(), [&](auto o) {
                    return o.key == obj.key;
                }) == objs3.end())
            {
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
            else
            {
                ASSERT_FALSE(cacheObj);
            }
        }
        for (auto& obj : objs2)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (auto& obj : objs3)
        {
            auto cacheObj = cache.get(obj.key, curSeq);
            ASSERT_FALSE(cacheObj);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        for (size_t i = 0; i < bObjs.size(); i++)
        {
            auto& obj = bObjs[i];
            auto cacheObj = cache.get(obj.key, curSeq);
            auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
                return o.key == obj.key;
            });
            auto delObj = std::find_if(objs3.begin(), objs3.end(), [&](auto o) {
                return o.key == obj.key;
            });
            if (delObj != objs3.end())
            {
                ASSERT_FALSE(cacheObj);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
            else if (newObj == objs1.end())
            {
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
                cacheObj = cache.get(obj.key, startSeq);
                ASSERT_TRUE(cacheObj);
                ASSERT_EQ(*cacheObj, obj.blob);
            }
            else
            {
                ASSERT_EQ(*cacheObj, newObj->blob);
                ASSERT_FALSE(cache.get(obj.key, startSeq));
            }
        }
    }
    cache.setFull();
    auto allObjs = bObjs;
    allObjs.clear();
    for (size_t i = 0; i < bObjs.size(); i++)
    {
        auto& obj = bObjs[i];
        auto cacheObj = cache.get(obj.key, curSeq);
        auto newObj = std::find_if(objs1.begin(), objs1.end(), [&](auto o) {
            return o.key == obj.key;
        });
        auto delObj = std::find_if(objs3.begin(), objs3.end(), [&](auto o) {
            return o.key == obj.key;
        });
        if (delObj != objs3.end())
        {
            ASSERT_FALSE(cacheObj);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
        else if (newObj == objs1.end())
        {
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            cacheObj = cache.get(obj.key, startSeq);
            ASSERT_TRUE(cacheObj);
            ASSERT_EQ(*cacheObj, obj.blob);
            allObjs.push_back(obj);
        }
        else
        {
            allObjs.push_back(*newObj);
            ASSERT_EQ(*cacheObj, newObj->blob);
            ASSERT_FALSE(cache.get(obj.key, startSeq));
        }
    }
    for (auto& obj : objs2)
    {
        allObjs.push_back(obj);
    }
    std::sort(allObjs.begin(), allObjs.end(), [](auto a, auto b) {
        return a.key < b.key;
    });
    std::optional<LedgerObject> succ = {{firstKey, {}}};
    size_t idx = 0;
    while ((succ = cache.getSuccessor(succ->key, curSeq)))
    {
        ASSERT_EQ(*succ, allObjs[idx++]);
    }
    ASSERT_EQ(idx, allObjs.size());
}

TEST_F(BackendTest, cacheIntegration)
{
    boost::asio::io_context ioc;
    std::optional<boost::asio::io_context::work> work;
    work.emplace(ioc);
    std::atomic_bool done = false;

    boost::asio::spawn(
        ioc, [&ioc, &done, &work](boost::asio::yield_context yield) {
            boost::log::core::get()->set_filter(
                util::log_severity >= util::Severity::WRN);
            std::string keyspace = "clio_test_" +
                std::to_string(std::chrono::system_clock::now()
                                   .time_since_epoch()
                                   .count());
            boost::json::object cassandraConfig{
                {"database",
                 {{"type", "cassandra"},
                  {"cassandra",
                   {{"contact_points", "127.0.0.1"},
                    {"port", 9042},
                    {"keyspace", keyspace.c_str()},
                    {"replication_factor", 1},
                    {"table_prefix", ""},
                    {"max_requests_outstanding", 1000},
                    {"indexer_key_shift", 2},
                    {"threads", 8}}}}}};
            std::vector<boost::json::object> configs = {cassandraConfig};
            for (auto& config : configs)
            {
                auto backend = make_Backend(ioc, util::Config{config});
                backend->cache().setFull();

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

                auto hexStringToBinaryString = [](auto const& hex) {
                    auto blob = ripple::strUnHex(hex);
                    std::string strBlob;
                    for (auto c : *blob)
                    {
                        strBlob += c;
                    }
                    return strBlob;
                };
                auto binaryStringToUint256 =
                    [](auto const& bin) -> ripple::uint256 {
                    ripple::uint256 uint;
                    return uint.fromVoid((void const*)bin.data());
                };
                auto ledgerInfoToBinaryString = [](auto const& info) {
                    auto blob = rpc::ledgerInfoToBlob(info, true);
                    std::string strBlob;
                    for (auto c : blob)
                    {
                        strBlob += c;
                    }
                    return strBlob;
                };

                std::string rawHeaderBlob = hexStringToBinaryString(rawHeader);
                std::string accountBlob = hexStringToBinaryString(accountHex);
                std::string accountIndexBlob =
                    hexStringToBinaryString(accountIndexHex);
                ripple::LedgerInfo lgrInfo =
                    deserializeHeader(ripple::makeSlice(rawHeaderBlob));

                backend->startWrites();
                backend->writeLedger(lgrInfo, std::move(rawHeaderBlob));
                backend->writeSuccessor(
                    uint256ToString(data::firstKey),
                    lgrInfo.seq,
                    uint256ToString(data::lastKey));
                ASSERT_TRUE(backend->finishWrites(lgrInfo.seq));
                {
                    auto rng = backend->fetchLedgerRange();
                    EXPECT_TRUE(rng.has_value());
                    EXPECT_EQ(rng->minSequence, rng->maxSequence);
                    EXPECT_EQ(rng->maxSequence, lgrInfo.seq);
                }
                {
                    auto seq = backend->fetchLatestLedgerSequence(yield);
                    EXPECT_TRUE(seq.has_value());
                    EXPECT_EQ(*seq, lgrInfo.seq);
                }

                {
                    auto retLgr =
                        backend->fetchLedgerBySequence(lgrInfo.seq, yield);
                    ASSERT_TRUE(retLgr.has_value());
                    EXPECT_EQ(retLgr->seq, lgrInfo.seq);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(lgrInfo),
                        rpc::ledgerInfoToBlob(*retLgr));
                }

                EXPECT_FALSE(
                    backend->fetchLedgerBySequence(lgrInfo.seq + 1, yield)
                        .has_value());
                auto lgrInfoOld = lgrInfo;

                auto lgrInfoNext = lgrInfo;
                lgrInfoNext.seq = lgrInfo.seq + 1;
                lgrInfoNext.parentHash = lgrInfo.hash;
                lgrInfoNext.hash++;
                lgrInfoNext.accountHash = ~lgrInfo.accountHash;
                {
                    std::string rawHeaderBlob =
                        ledgerInfoToBinaryString(lgrInfoNext);

                    backend->startWrites();
                    backend->writeLedger(lgrInfoNext, std::move(rawHeaderBlob));
                    ASSERT_TRUE(backend->finishWrites(lgrInfoNext.seq));
                }
                {
                    auto rng = backend->fetchLedgerRange();
                    EXPECT_TRUE(rng.has_value());
                    EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
                    EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
                }
                {
                    auto seq = backend->fetchLatestLedgerSequence(yield);
                    EXPECT_EQ(seq, lgrInfoNext.seq);
                }
                {
                    auto retLgr =
                        backend->fetchLedgerBySequence(lgrInfoNext.seq, yield);
                    EXPECT_TRUE(retLgr.has_value());
                    EXPECT_EQ(retLgr->seq, lgrInfoNext.seq);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfoNext));
                    EXPECT_NE(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfoOld));
                    retLgr = backend->fetchLedgerBySequence(
                        lgrInfoNext.seq - 1, yield);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfoOld));

                    EXPECT_NE(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfoNext));
                    retLgr = backend->fetchLedgerBySequence(
                        lgrInfoNext.seq - 2, yield);
                    EXPECT_FALSE(
                        backend
                            ->fetchLedgerBySequence(lgrInfoNext.seq - 2, yield)
                            .has_value());

                    auto txns = backend->fetchAllTransactionsInLedger(
                        lgrInfoNext.seq, yield);
                    EXPECT_EQ(txns.size(), 0);
                    auto hashes = backend->fetchAllTransactionHashesInLedger(
                        lgrInfoNext.seq, yield);
                    EXPECT_EQ(hashes.size(), 0);
                }

                {
                    backend->startWrites();
                    lgrInfoNext.seq = lgrInfoNext.seq + 1;
                    lgrInfoNext.txHash = ~lgrInfo.txHash;
                    lgrInfoNext.accountHash =
                        lgrInfoNext.accountHash ^ lgrInfoNext.txHash;
                    lgrInfoNext.parentHash = lgrInfoNext.hash;
                    lgrInfoNext.hash++;

                    backend->writeLedger(
                        lgrInfoNext, ledgerInfoToBinaryString(lgrInfoNext));
                    backend->writeLedgerObject(
                        std::string{accountIndexBlob},
                        lgrInfoNext.seq,
                        std::string{accountBlob});
                    auto key =
                        ripple::uint256::fromVoidChecked(accountIndexBlob);
                    backend->cache().update(
                        {{*key, {accountBlob.begin(), accountBlob.end()}}},
                        lgrInfoNext.seq);
                    backend->writeSuccessor(
                        uint256ToString(data::firstKey),
                        lgrInfoNext.seq,
                        std::string{accountIndexBlob});
                    backend->writeSuccessor(
                        std::string{accountIndexBlob},
                        lgrInfoNext.seq,
                        uint256ToString(data::lastKey));

                    ASSERT_TRUE(backend->finishWrites(lgrInfoNext.seq));
                }

                {
                    auto rng = backend->fetchLedgerRange();
                    EXPECT_TRUE(rng);
                    EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
                    EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
                    auto retLgr =
                        backend->fetchLedgerBySequence(lgrInfoNext.seq, yield);
                    EXPECT_TRUE(retLgr);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfoNext));
                    ripple::uint256 key256;
                    EXPECT_TRUE(key256.parseHex(accountIndexHex));
                    auto obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq, yield);
                    EXPECT_TRUE(obj);
                    EXPECT_STREQ(
                        (const char*)obj->data(),
                        (const char*)accountBlob.data());
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq + 1, yield);
                    EXPECT_TRUE(obj);
                    EXPECT_STREQ(
                        (const char*)obj->data(),
                        (const char*)accountBlob.data());
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoOld.seq - 1, yield);
                    EXPECT_FALSE(obj);
                }
                // obtain a time-based seed:
                unsigned seed =
                    std::chrono::system_clock::now().time_since_epoch().count();
                std::string accountBlobOld = accountBlob;
                {
                    backend->startWrites();
                    lgrInfoNext.seq = lgrInfoNext.seq + 1;
                    lgrInfoNext.parentHash = lgrInfoNext.hash;
                    lgrInfoNext.hash++;
                    lgrInfoNext.txHash =
                        lgrInfoNext.txHash ^ lgrInfoNext.accountHash;
                    lgrInfoNext.accountHash =
                        ~(lgrInfoNext.accountHash ^ lgrInfoNext.txHash);

                    backend->writeLedger(
                        lgrInfoNext, ledgerInfoToBinaryString(lgrInfoNext));
                    std::shuffle(
                        accountBlob.begin(),
                        accountBlob.end(),
                        std::default_random_engine(seed));
                    auto key =
                        ripple::uint256::fromVoidChecked(accountIndexBlob);
                    backend->cache().update(
                        {{*key, {accountBlob.begin(), accountBlob.end()}}},
                        lgrInfoNext.seq);
                    backend->writeLedgerObject(
                        std::string{accountIndexBlob},
                        lgrInfoNext.seq,
                        std::string{accountBlob});

                    ASSERT_TRUE(backend->finishWrites(lgrInfoNext.seq));
                }
                {
                    auto rng = backend->fetchLedgerRange();
                    EXPECT_TRUE(rng);
                    EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
                    EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
                    auto retLgr =
                        backend->fetchLedgerBySequence(lgrInfoNext.seq, yield);
                    EXPECT_TRUE(retLgr);

                    ripple::uint256 key256;
                    EXPECT_TRUE(key256.parseHex(accountIndexHex));
                    auto obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq, yield);
                    EXPECT_TRUE(obj);
                    EXPECT_STREQ(
                        (const char*)obj->data(),
                        (const char*)accountBlob.data());
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq + 1, yield);
                    EXPECT_TRUE(obj);
                    EXPECT_STREQ(
                        (const char*)obj->data(),
                        (const char*)accountBlob.data());
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq - 1, yield);
                    EXPECT_TRUE(obj);
                    EXPECT_STREQ(
                        (const char*)obj->data(),
                        (const char*)accountBlobOld.data());
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoOld.seq - 1, yield);
                    EXPECT_FALSE(obj);
                }
                {
                    backend->startWrites();
                    lgrInfoNext.seq = lgrInfoNext.seq + 1;
                    lgrInfoNext.parentHash = lgrInfoNext.hash;
                    lgrInfoNext.hash++;
                    lgrInfoNext.txHash =
                        lgrInfoNext.txHash ^ lgrInfoNext.accountHash;
                    lgrInfoNext.accountHash =
                        ~(lgrInfoNext.accountHash ^ lgrInfoNext.txHash);

                    backend->writeLedger(
                        lgrInfoNext, ledgerInfoToBinaryString(lgrInfoNext));
                    auto key =
                        ripple::uint256::fromVoidChecked(accountIndexBlob);
                    backend->cache().update({{*key, {}}}, lgrInfoNext.seq);
                    backend->writeLedgerObject(
                        std::string{accountIndexBlob},
                        lgrInfoNext.seq,
                        std::string{});
                    backend->writeSuccessor(
                        uint256ToString(data::firstKey),
                        lgrInfoNext.seq,
                        uint256ToString(data::lastKey));

                    ASSERT_TRUE(backend->finishWrites(lgrInfoNext.seq));
                }
                {
                    auto rng = backend->fetchLedgerRange();
                    EXPECT_TRUE(rng);
                    EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
                    EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
                    auto retLgr =
                        backend->fetchLedgerBySequence(lgrInfoNext.seq, yield);
                    EXPECT_TRUE(retLgr);

                    ripple::uint256 key256;
                    EXPECT_TRUE(key256.parseHex(accountIndexHex));
                    auto obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq, yield);
                    EXPECT_FALSE(obj);
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq + 1, yield);
                    EXPECT_FALSE(obj);
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoNext.seq - 2, yield);
                    EXPECT_TRUE(obj);
                    EXPECT_STREQ(
                        (const char*)obj->data(),
                        (const char*)accountBlobOld.data());
                    obj = backend->fetchLedgerObject(
                        key256, lgrInfoOld.seq - 1, yield);
                    EXPECT_FALSE(obj);
                }

                auto generateObjects = [](size_t numObjects,
                                          uint32_t ledgerSequence) {
                    std::vector<std::pair<std::string, std::string>> res{
                        numObjects};
                    ripple::uint256 key;
                    key = ledgerSequence * 100000;

                    for (auto& blob : res)
                    {
                        ++key;
                        std::string keyStr{(const char*)key.data(), key.size()};
                        blob.first = keyStr;
                        blob.second = std::to_string(ledgerSequence) + keyStr;
                    }
                    return res;
                };
                auto updateObjects = [](uint32_t ledgerSequence, auto objs) {
                    for (auto& [key, obj] : objs)
                    {
                        obj = std::to_string(ledgerSequence) + obj;
                    }
                    return objs;
                };

                auto generateNextLedger = [seed](auto lgrInfo) {
                    ++lgrInfo.seq;
                    lgrInfo.parentHash = lgrInfo.hash;
                    std::srand(std::time(nullptr));
                    std::shuffle(
                        lgrInfo.txHash.begin(),
                        lgrInfo.txHash.end(),
                        std::default_random_engine(seed));
                    std::shuffle(
                        lgrInfo.accountHash.begin(),
                        lgrInfo.accountHash.end(),
                        std::default_random_engine(seed));
                    std::shuffle(
                        lgrInfo.hash.begin(),
                        lgrInfo.hash.end(),
                        std::default_random_engine(seed));
                    return lgrInfo;
                };
                auto writeLedger = [&](auto lgrInfo, auto objs, auto state) {
                    backend->startWrites();

                    backend->writeLedger(
                        lgrInfo, std::move(ledgerInfoToBinaryString(lgrInfo)));
                    std::vector<data::LedgerObject> cacheUpdates;
                    for (auto [key, obj] : objs)
                    {
                        backend->writeLedgerObject(
                            std::string{key}, lgrInfo.seq, std::string{obj});
                        auto key256 = ripple::uint256::fromVoidChecked(key);
                        cacheUpdates.push_back(
                            {*key256, {obj.begin(), obj.end()}});
                    }
                    backend->cache().update(cacheUpdates, lgrInfo.seq);
                    if (state.count(lgrInfo.seq - 1) == 0 ||
                        std::find_if(
                            state[lgrInfo.seq - 1].begin(),
                            state[lgrInfo.seq - 1].end(),
                            [&](auto obj) {
                                return obj.first == objs[0].first;
                            }) == state[lgrInfo.seq - 1].end())
                    {
                        for (size_t i = 0; i < objs.size(); ++i)
                        {
                            if (i + 1 < objs.size())
                                backend->writeSuccessor(
                                    std::string{objs[i].first},
                                    lgrInfo.seq,
                                    std::string{objs[i + 1].first});
                            else
                                backend->writeSuccessor(
                                    std::string{objs[i].first},
                                    lgrInfo.seq,
                                    uint256ToString(data::lastKey));
                        }
                        if (state.count(lgrInfo.seq - 1))
                            backend->writeSuccessor(
                                std::string{
                                    state[lgrInfo.seq - 1].back().first},
                                lgrInfo.seq,
                                std::string{objs[0].first});
                        else
                            backend->writeSuccessor(
                                uint256ToString(data::firstKey),
                                lgrInfo.seq,
                                std::string{objs[0].first});
                    }

                    ASSERT_TRUE(backend->finishWrites(lgrInfo.seq));
                };

                auto checkLedger = [&](auto lgrInfo, auto objs) {
                    auto rng = backend->fetchLedgerRange();
                    auto seq = lgrInfo.seq;
                    EXPECT_TRUE(rng);
                    EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
                    EXPECT_GE(rng->maxSequence, seq);
                    auto retLgr = backend->fetchLedgerBySequence(seq, yield);
                    EXPECT_TRUE(retLgr);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfo));
                    retLgr = backend->fetchLedgerByHash(lgrInfo.hash, yield);
                    EXPECT_TRUE(retLgr);
                    EXPECT_EQ(
                        rpc::ledgerInfoToBlob(*retLgr),
                        rpc::ledgerInfoToBlob(lgrInfo));
                    std::vector<ripple::uint256> keys;
                    for (auto [key, obj] : objs)
                    {
                        auto retObj = backend->fetchLedgerObject(
                            binaryStringToUint256(key), seq, yield);
                        if (obj.size())
                        {
                            ASSERT_TRUE(retObj.has_value());
                            EXPECT_STREQ(
                                (const char*)obj.data(),
                                (const char*)retObj->data());
                        }
                        else
                        {
                            ASSERT_FALSE(retObj.has_value());
                        }
                        keys.push_back(binaryStringToUint256(key));
                    }

                    {
                        auto retObjs =
                            backend->fetchLedgerObjects(keys, seq, yield);
                        ASSERT_EQ(retObjs.size(), objs.size());

                        for (size_t i = 0; i < keys.size(); ++i)
                        {
                            auto [key, obj] = objs[i];
                            auto retObj = retObjs[i];
                            if (obj.size())
                            {
                                ASSERT_TRUE(retObj.size());
                                EXPECT_STREQ(
                                    (const char*)obj.data(),
                                    (const char*)retObj.data());
                            }
                            else
                            {
                                ASSERT_FALSE(retObj.size());
                            }
                        }
                    }
                    data::LedgerPage page;
                    std::vector<data::LedgerObject> retObjs;
                    size_t numLoops = 0;
                    do
                    {
                        uint32_t limit = 10;
                        page = backend->fetchLedgerPage(
                            page.cursor, seq, limit, false, yield);
                        // if (page.cursor)
                        //    EXPECT_EQ(page.objects.size(), limit);
                        retObjs.insert(
                            retObjs.end(),
                            page.objects.begin(),
                            page.objects.end());
                        ++numLoops;
                    } while (page.cursor);
                    for (auto obj : objs)
                    {
                        bool found = false;
                        for (auto retObj : retObjs)
                        {
                            if (ripple::strHex(obj.first) ==
                                ripple::strHex(retObj.key))
                            {
                                found = true;
                                ASSERT_EQ(
                                    ripple::strHex(obj.second),
                                    ripple::strHex(retObj.blob));
                            }
                        }
                        if (found != (obj.second.size() != 0))
                            ASSERT_EQ(found, obj.second.size() != 0);
                    }
                };

                std::map<
                    uint32_t,
                    std::vector<std::pair<std::string, std::string>>>
                    state;
                std::map<uint32_t, ripple::LedgerInfo> lgrInfos;
                for (size_t i = 0; i < 10; ++i)
                {
                    lgrInfoNext = generateNextLedger(lgrInfoNext);
                    auto objs = generateObjects(25, lgrInfoNext.seq);
                    EXPECT_EQ(objs.size(), 25);
                    EXPECT_NE(objs[0], objs[1]);
                    std::sort(objs.begin(), objs.end());
                    state[lgrInfoNext.seq] = objs;
                    writeLedger(lgrInfoNext, objs, state);
                    lgrInfos[lgrInfoNext.seq] = lgrInfoNext;
                }

                std::vector<std::pair<std::string, std::string>> objs;
                for (size_t i = 0; i < 10; ++i)
                {
                    lgrInfoNext = generateNextLedger(lgrInfoNext);
                    if (!objs.size())
                        objs = generateObjects(25, lgrInfoNext.seq);
                    else
                        objs = updateObjects(lgrInfoNext.seq, objs);
                    EXPECT_EQ(objs.size(), 25);
                    EXPECT_NE(objs[0], objs[1]);
                    std::sort(objs.begin(), objs.end());
                    state[lgrInfoNext.seq] = objs;
                    writeLedger(lgrInfoNext, objs, state);
                    lgrInfos[lgrInfoNext.seq] = lgrInfoNext;
                }

                auto flatten = [&](uint32_t max) {
                    std::vector<std::pair<std::string, std::string>> flat;
                    std::map<std::string, std::string> objs;
                    for (auto [seq, diff] : state)
                    {
                        for (auto [k, v] : diff)
                        {
                            if (seq > max)
                            {
                                if (objs.count(k) == 0)
                                    objs[k] = "";
                            }
                            else
                            {
                                objs[k] = v;
                            }
                        }
                    }
                    for (auto [key, value] : objs)
                    {
                        flat.push_back(std::make_pair(key, value));
                    }
                    return flat;
                };

                for (auto [seq, diff] : state)
                {
                    auto flat = flatten(seq);
                    checkLedger(lgrInfos[seq], flat);
                }
            }

            done = true;
            work.reset();
        });

    ioc.run();
}
