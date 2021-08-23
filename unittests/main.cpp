#include <algorithm>
#include <backend/DBHelpers.h>
#include <gtest/gtest.h>
#include <rpc/RPCHelpers.h>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <backend/BackendFactory.h>
#include <backend/BackendInterface.h>

// Demonstrate some basic assertions.
TEST(BackendTest, Basic)
{
    boost::log::core::get()->set_filter(
        boost::log::trivial::severity >= boost::log::trivial::warning);
    std::string keyspace =
        "oceand_test_" +
        std::to_string(
            std::chrono::system_clock::now().time_since_epoch().count());
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
    boost::json::object postgresConfig{
        {"database",
         {{"type", "postgres"},
          {"postgres",
           {{"contact_point", "127.0.0.1"},
            {"username", "postgres"},
            {"database", keyspace.c_str()},
            {"password", "postgres"},
            {"indexer_key_shift", 2},
            {"threads", 8}}}}}};
    std::vector<boost::json::object> configs = {
        cassandraConfig, postgresConfig};
    for (auto& config : configs)
    {
        std::cout << keyspace << std::endl;
        auto backend = Backend::make_Backend(config);
        backend->open(false);

        std::string rawHeader =
            "03C3141A01633CD656F91B4EBB5EB89B791BD34DBC8A04BB6F407C5335BC54351E"
            "DD73"
            "3898497E809E04074D14D271E4832D7888754F9230800761563A292FA2315A6DB6"
            "FE30"
            "CC5909B285080FCD6773CC883F9FE0EE4D439340AC592AADB973ED3CF53E2232B3"
            "3EF5"
            "7CECAC2816E3122816E31A0A00F8377CD95DFA484CFAE282656A58CE5AA29652EF"
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
            auto blob = RPC::ledgerInfoToBlob(info);
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
        backend->writeLedger(lgrInfo, std::move(rawHeaderBlob), true);
        ASSERT_TRUE(backend->finishWrites(lgrInfo.seq));
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
            EXPECT_TRUE(retLgr.has_value());
            EXPECT_EQ(retLgr->seq, lgrInfo.seq);
            EXPECT_EQ(
                RPC::ledgerInfoToBlob(lgrInfo), RPC::ledgerInfoToBlob(*retLgr));
        }

        EXPECT_FALSE(
            backend->fetchLedgerBySequence(lgrInfo.seq + 1).has_value());
        auto lgrInfoOld = lgrInfo;

        auto lgrInfoNext = lgrInfo;
        lgrInfoNext.seq = lgrInfo.seq + 1;
        lgrInfoNext.parentHash = lgrInfo.hash;
        lgrInfoNext.hash++;
        lgrInfoNext.accountHash = ~lgrInfo.accountHash;
        {
            std::string rawHeaderBlob = ledgerInfoToBinaryString(lgrInfoNext);

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
            auto seq = backend->fetchLatestLedgerSequence();
            EXPECT_EQ(seq, lgrInfoNext.seq);
        }
        {
            auto retLgr = backend->fetchLedgerBySequence(lgrInfoNext.seq);
            EXPECT_TRUE(retLgr.has_value());
            EXPECT_EQ(retLgr->seq, lgrInfoNext.seq);
            EXPECT_EQ(
                RPC::ledgerInfoToBlob(*retLgr),
                RPC::ledgerInfoToBlob(lgrInfoNext));
            EXPECT_NE(
                RPC::ledgerInfoToBlob(*retLgr),
                RPC::ledgerInfoToBlob(lgrInfoOld));
            retLgr = backend->fetchLedgerBySequence(lgrInfoNext.seq - 1);
            EXPECT_EQ(
                RPC::ledgerInfoToBlob(*retLgr),
                RPC::ledgerInfoToBlob(lgrInfoOld));

            EXPECT_NE(
                RPC::ledgerInfoToBlob(*retLgr),
                RPC::ledgerInfoToBlob(lgrInfoNext));
            retLgr = backend->fetchLedgerBySequence(lgrInfoNext.seq - 2);
            EXPECT_FALSE(backend->fetchLedgerBySequence(lgrInfoNext.seq - 2)
                             .has_value());

            auto txns = backend->fetchAllTransactionsInLedger(lgrInfoNext.seq);
            EXPECT_EQ(txns.size(), 0);
            auto hashes =
                backend->fetchAllTransactionHashesInLedger(lgrInfoNext.seq);
            EXPECT_EQ(hashes.size(), 0);
        }

        // the below dummy data is not expected to be consistent. The metadata
        // string does represent valid metadata. Don't assume though that the
        // transaction or its hash correspond to the metadata, or anything like
        // that. These tests are purely binary tests to make sure the same data
        // that goes in, comes back out
        std::string metaHex =
            "201C0000001AF8E411006F560A3E08122A05AC91DEFA87052B0554E4A29B46"
            "3A27642EBB060B6052196592EEE72200000000240480FDB52503CE1A863300"
            "000000000000003400000000000000005529983CBAED30F547471452921C3C"
            "6B9F9685F292F6291000EED0A44413AF18C250101AC09600F4B502C8F7F830"
            "F80B616DCB6F3970CB79AB70975A05ED5B66860B9564400000001FE217CB65"
            "D54B640B31521B05000000000000000000000000434E5900000000000360E3"
            "E0751BD9A566CD03FA6CAFC78118B82BA081142252F328CF91263417762570"
            "D67220CCB33B1370E1E1E3110064561AC09600F4B502C8F7F830F80B616DCB"
            "6F3970CB79AB70975A05ED33DF783681E8365A05ED33DF783681581AC09600"
            "F4B502C8F7F830F80B616DCB6F3970CB79AB70975A05ED33DF783681031100"
            "0000000000000000000000434E59000000000004110360E3E0751BD9A566CD"
            "03FA6CAFC78118B82BA0E1E1E4110064561AC09600F4B502C8F7F830F80B61"
            "6DCB6F3970CB79AB70975A05ED5B66860B95E72200000000365A05ED5B6686"
            "0B95581AC09600F4B502C8F7F830F80B616DCB6F3970CB79AB70975A05ED5B"
            "66860B95011100000000000000000000000000000000000000000211000000"
            "00000000000000000000000000000000000311000000000000000000000000"
            "434E59000000000004110360E3E0751BD9A566CD03FA6CAFC78118B82BA0E1"
            "E1E311006F5647B05E66DE9F3DF2689E8F4CE6126D3136B6C5E79587F9D24B"
            "D71A952B0852BAE8240480FDB950101AC09600F4B502C8F7F830F80B616DCB"
            "6F3970CB79AB70975A05ED33DF78368164400000033C83A95F65D59D9A6291"
            "9C2D18000000000000000000000000434E5900000000000360E3E0751BD9A5"
            "66CD03FA6CAFC78118B82BA081142252F328CF91263417762570D67220CCB3"
            "3B1370E1E1E511006456AEA3074F10FE15DAC592F8A0405C61FB7D4C98F588"
            "C2D55C84718FAFBBD2604AE722000000003100000000000000003200000000"
            "0000000058AEA3074F10FE15DAC592F8A0405C61FB7D4C98F588C2D55C8471"
            "8FAFBBD2604A82142252F328CF91263417762570D67220CCB33B1370E1E1E5"
            "1100612503CE1A8755CE935137F8C6C8DEF26B5CD93BE18105CA83F65E1E90"
            "CEC546F562D25957DC0856E0311EB450B6177F969B94DBDDA83E99B7A0576A"
            "CD9079573876F16C0C004F06E6240480FDB9624000000005FF0E2BE1E72200"
            "000000240480FDBA2D00000005624000000005FF0E1F81142252F328CF9126"
            "3417762570D67220CCB33B1370E1E1F1031000";
        std::string txnHex =
            "1200072200000000240480FDB920190480FDB5201B03CE1A8964400000033C"
            "83A95F65D59D9A62919C2D18000000000000000000000000434E5900000000"
            "000360E3E0751BD9A566CD03FA6CAFC78118B82BA068400000000000000C73"
            "21022D40673B44C82DEE1DDB8B9BB53DCCE4F97B27404DB850F068DD91D685"
            "E337EA7446304402202EA6B702B48B39F2197112382838F92D4C02948E9911"
            "FE6B2DEBCF9183A426BC022005DAC06CD4517E86C2548A80996019F3AC60A0"
            "9EED153BF60C992930D68F09F981142252F328CF91263417762570D67220CC"
            "B33B1370";
        std::string hashHex =
            "0A81FB3D6324C2DCF73131505C6E4DC67981D7FC39F5E9574CEC4B1F22D28BF7";

        // this account is not related to the above transaction and metadata
        std::string accountHex =
            "1100612200000000240480FDBC2503CE1A872D0000000555516931B2AD018EFFBE"
            "17C5"
            "C9DCCF872F36837C2C6136ACF80F2A24079CF81FD0624000000005FF0E07811422"
            "52F3"
            "28CF91263417762570D67220CCB33B1370";
        std::string accountIndexHex =
            "E0311EB450B6177F969B94DBDDA83E99B7A0576ACD9079573876F16C0C004F06";

        std::string metaBlob = hexStringToBinaryString(metaHex);
        std::string txnBlob = hexStringToBinaryString(txnHex);
        std::string hashBlob = hexStringToBinaryString(hashHex);
        std::string accountBlob = hexStringToBinaryString(accountHex);
        std::string accountIndexBlob = hexStringToBinaryString(accountIndexHex);
        std::vector<ripple::AccountID> affectedAccounts;

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
            auto accountsSet = txMeta.getAffectedAccounts(journal);
            for (auto& a : accountsSet)
            {
                affectedAccounts.push_back(a);
            }

            std::vector<AccountTransactionsData> accountTxData;
            accountTxData.emplace_back(txMeta, hash256, journal);
            backend->writeLedger(
                lgrInfoNext, std::move(ledgerInfoToBinaryString(lgrInfoNext)));
            backend->writeTransaction(
                std::move(std::string{hashBlob}),
                lgrInfoNext.seq,
                std::move(std::string{txnBlob}),
                std::move(std::string{metaBlob}));
            backend->writeAccountTransactions(std::move(accountTxData));
            backend->writeLedgerObject(
                std::move(std::string{accountIndexBlob}),
                lgrInfoNext.seq,
                std::move(std::string{accountBlob}));

            ASSERT_TRUE(backend->finishWrites(lgrInfoNext.seq));
        }

        {
            auto rng = backend->fetchLedgerRange();
            EXPECT_TRUE(rng);
            EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
            EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
            auto retLgr = backend->fetchLedgerBySequence(lgrInfoNext.seq);
            EXPECT_TRUE(retLgr);
            EXPECT_EQ(
                RPC::ledgerInfoToBlob(*retLgr),
                RPC::ledgerInfoToBlob(lgrInfoNext));
            auto txns = backend->fetchAllTransactionsInLedger(lgrInfoNext.seq);
            EXPECT_EQ(txns.size(), 1);
            EXPECT_STREQ(
                (const char*)txns[0].transaction.data(),
                (const char*)txnBlob.data());
            EXPECT_STREQ(
                (const char*)txns[0].metadata.data(),
                (const char*)metaBlob.data());
            auto hashes =
                backend->fetchAllTransactionHashesInLedger(lgrInfoNext.seq);
            EXPECT_EQ(hashes.size(), 1);
            EXPECT_EQ(ripple::strHex(hashes[0]), hashHex);
            for (auto& a : affectedAccounts)
            {
                auto [txns, cursor] = backend->fetchAccountTransactions(a, 100);
                EXPECT_EQ(txns.size(), 1);
                EXPECT_EQ(txns[0], txns[0]);
                EXPECT_FALSE(cursor);
            }

            ripple::uint256 key256;
            EXPECT_TRUE(key256.parseHex(accountIndexHex));
            auto obj = backend->fetchLedgerObject(key256, lgrInfoNext.seq);
            EXPECT_TRUE(obj);
            EXPECT_STREQ(
                (const char*)obj->data(), (const char*)accountBlob.data());
            obj = backend->fetchLedgerObject(key256, lgrInfoNext.seq + 1);
            EXPECT_TRUE(obj);
            EXPECT_STREQ(
                (const char*)obj->data(), (const char*)accountBlob.data());
            obj = backend->fetchLedgerObject(key256, lgrInfoOld.seq - 1);
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
            lgrInfoNext.txHash = lgrInfoNext.txHash ^ lgrInfoNext.accountHash;
            lgrInfoNext.accountHash =
                ~(lgrInfoNext.accountHash ^ lgrInfoNext.txHash);

            backend->writeLedger(
                lgrInfoNext, std::move(ledgerInfoToBinaryString(lgrInfoNext)));
            std::shuffle(
                accountBlob.begin(),
                accountBlob.end(),
                std::default_random_engine(seed));
            backend->writeLedgerObject(
                std::move(std::string{accountIndexBlob}),
                lgrInfoNext.seq,
                std::move(std::string{accountBlob}));

            ASSERT_TRUE(backend->finishWrites(lgrInfoNext.seq));
        }
        {
            auto rng = backend->fetchLedgerRange();
            EXPECT_TRUE(rng);
            EXPECT_EQ(rng->minSequence, lgrInfoOld.seq);
            EXPECT_EQ(rng->maxSequence, lgrInfoNext.seq);
            auto retLgr = backend->fetchLedgerBySequence(lgrInfoNext.seq);
            EXPECT_TRUE(retLgr);
            EXPECT_EQ(
                RPC::ledgerInfoToBlob(*retLgr),
                RPC::ledgerInfoToBlob(lgrInfoNext));
            auto txns = backend->fetchAllTransactionsInLedger(lgrInfoNext.seq);
            EXPECT_EQ(txns.size(), 0);

            ripple::uint256 key256;
            EXPECT_TRUE(key256.parseHex(accountIndexHex));
            auto obj = backend->fetchLedgerObject(key256, lgrInfoNext.seq);
            EXPECT_TRUE(obj);
            EXPECT_STREQ(
                (const char*)obj->data(), (const char*)accountBlob.data());
            obj = backend->fetchLedgerObject(key256, lgrInfoNext.seq + 1);
            EXPECT_TRUE(obj);
            EXPECT_STREQ(
                (const char*)obj->data(), (const char*)accountBlob.data());
            obj = backend->fetchLedgerObject(key256, lgrInfoNext.seq - 1);
            EXPECT_TRUE(obj);
            EXPECT_STREQ(
                (const char*)obj->data(), (const char*)accountBlobOld.data());
            obj = backend->fetchLedgerObject(key256, lgrInfoOld.seq - 1);
            EXPECT_FALSE(obj);
        }

        auto generateObjects = [seed](
                                   size_t numObjects, uint32_t ledgerSequence) {
            std::vector<std::pair<std::string, std::string>> res{numObjects};
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
        auto generateTxns = [seed](size_t numTxns, uint32_t ledgerSequence) {
            std::vector<std::tuple<std::string, std::string, std::string>> res{
                numTxns};
            ripple::uint256 base;
            base = ledgerSequence * 100000;
            for (auto& blob : res)
            {
                ++base;
                std::string hashStr{(const char*)base.data(), base.size()};
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
        auto generateAccountTx = [&](uint32_t ledgerSequence, auto txns) {
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
        auto writeLedger =
            [&](auto lgrInfo, auto txns, auto objs, auto accountTx) {
                std::cout << "writing ledger = " << std::to_string(lgrInfo.seq);
                backend->startWrites();

                backend->writeLedger(
                    lgrInfo, std::move(ledgerInfoToBinaryString(lgrInfo)));
                for (auto [hash, txn, meta] : txns)
                {
                    backend->writeTransaction(
                        std::move(hash),
                        lgrInfo.seq,
                        std::move(txn),
                        std::move(meta));
                }
                for (auto [key, obj] : objs)
                {
                    std::optional<ripple::uint256> bookDir;
                    if (isOffer(obj.data()))
                        bookDir = getBook(obj);
                    backend->writeLedgerObject(
                        std::move(key), lgrInfo.seq, std::move(obj));
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
            auto retLgr = backend->fetchLedgerBySequence(seq);
            EXPECT_TRUE(retLgr);
            EXPECT_EQ(
                RPC::ledgerInfoToBlob(*retLgr), RPC::ledgerInfoToBlob(lgrInfo));
            // retLgr = backend->fetchLedgerByHash(lgrInfo.hash);
            // EXPECT_TRUE(retLgr);
            // EXPECT_EQ(RPC::ledgerInfoToBlob(*retLgr),
            // RPC::ledgerInfoToBlob(lgrInfo));
            auto retTxns = backend->fetchAllTransactionsInLedger(seq);
            for (auto [hash, txn, meta] : txns)
            {
                bool found = false;
                for (auto [retTxn, retMeta, retSeq] : retTxns)
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
                std::vector<Backend::TransactionAndMetadata> retData;
                std::optional<Backend::AccountTransactionsCursor> cursor;
                do
                {
                    uint32_t limit = 10;
                    auto [txns, retCursor] = backend->fetchAccountTransactions(
                        account, limit, false, cursor);
                    if (retCursor)
                        EXPECT_EQ(txns.size(), limit);
                    retData.insert(retData.end(), txns.begin(), txns.end());
                    cursor = retCursor;
                } while (cursor);
                EXPECT_EQ(retData.size(), data.size());
                for (size_t i = 0; i < retData.size(); ++i)
                {
                    auto [txn, meta, seq] = retData[i];
                    auto [hash, expTxn, expMeta] = data[i];
                    EXPECT_STREQ(
                        (const char*)txn.data(), (const char*)expTxn.data());
                    EXPECT_STREQ(
                        (const char*)meta.data(), (const char*)expMeta.data());
                }
            }
            for (auto [key, obj] : objs)
            {
                auto retObj =
                    backend->fetchLedgerObject(binaryStringToUint256(key), seq);
                if (obj.size())
                {
                    ASSERT_TRUE(retObj.has_value());
                    EXPECT_STREQ(
                        (const char*)obj.data(), (const char*)retObj->data());
                }
                else
                {
                    ASSERT_FALSE(retObj.has_value());
                }
            }
            Backend::LedgerPage page;
            std::vector<Backend::LedgerObject> retObjs;
            size_t numLoops = 0;
            do
            {
                uint32_t limit = 10;
                page = backend->fetchLedgerPage(page.cursor, seq, limit);
                if (page.cursor)
                    EXPECT_EQ(page.objects.size(), limit);
                retObjs.insert(
                    retObjs.end(), page.objects.begin(), page.objects.end());
                ++numLoops;
                ASSERT_FALSE(page.warning.has_value());
            } while (page.cursor);
            for (auto obj : objs)
            {
                bool found = false;
                bool correct = false;
                for (auto retObj : retObjs)
                {
                    if (ripple::strHex(obj.first) == ripple::strHex(retObj.key))
                    {
                        found = true;
                        ASSERT_EQ(
                            ripple::strHex(obj.second),
                            ripple::strHex(retObj.blob));
                    }
                }
                ASSERT_EQ(found, obj.second.size() != 0);
            }
        };

        std::map<uint32_t, std::vector<std::pair<std::string, std::string>>>
            state;
        std::map<
            uint32_t,
            std::vector<std::tuple<std::string, std::string, std::string>>>
            allTxns;
        std::unordered_map<std::string, std::pair<std::string, std::string>>
            allTxnsMap;
        std::
            map<uint32_t, std::map<ripple::AccountID, std::vector<std::string>>>
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
                            (const char*)rec.txHash.data(), rec.txHash.size()});
                }
            }
            EXPECT_EQ(objs.size(), 25);
            EXPECT_NE(objs[0], objs[1]);
            EXPECT_EQ(txns.size(), 10);
            EXPECT_NE(txns[0], txns[1]);
            writeLedger(lgrInfoNext, txns, objs, accountTx);
            state[lgrInfoNext.seq] = objs;
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
                            (const char*)rec.txHash.data(), rec.txHash.size()});
                }
            }
            EXPECT_EQ(objs.size(), 25);
            EXPECT_NE(objs[0], objs[1]);
            EXPECT_EQ(txns.size(), 10);
            EXPECT_NE(txns[0], txns[1]);
            writeLedger(lgrInfoNext, txns, objs, accountTx);
            state[lgrInfoNext.seq] = objs;
            allTxns[lgrInfoNext.seq] = txns;
            lgrInfos[lgrInfoNext.seq] = lgrInfoNext;
            for (auto& [hash, txn, meta] : txns)
            {
                allTxnsMap[hash] = std::make_pair(txn, meta);
            }
        }
        std::cout << "WROTE ALL OBJECTS" << std::endl;
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
                std::vector<std::tuple<std::string, std::string, std::string>>>
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
            std::cout << "flatteneing" << std::endl;
            auto flat = flatten(seq);
            std::cout << "flattened" << std::endl;
            checkLedger(
                lgrInfos[seq], allTxns[seq], flat, flattenAccountTx(seq));
            std::cout << "checked" << std::endl;
        }
    }
}

