#include "data/DBHelpers.hpp"
#include "data/LedgerCache.hpp"
#include "data/Types.hpp"
#include "data/cassandra/Handle.hpp"
#include "data/cassandra/Schema.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "etl/MPTHelpers.hpp"
#include "etl/Models.hpp"
#include "etl/impl/ext/MPT.hpp"
#include "migration/MigrationManagerInterface.hpp"
#include "migration/MigratiorStatus.hpp"
#include "migration/cassandra/CassandraMigrationBackend.hpp"
#include "migration/cassandra/MPTTransactionHistoryMigrator.hpp"
#include "migration/impl/MigrationManagerBase.hpp"
#include "migration/impl/MigratorsRegister.hpp"
#include "util/LedgerUtils.hpp"
#include "util/MockPrometheus.hpp"
#include "util/StringUtils.hpp"
#include "util/TestObject.hpp"
#include "util/config/ConfigConstraints.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"

#include <TestGlobals.hpp>
#include <gtest/gtest.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/STArray.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TER.h>
#include <xrpl/protocol/TxFormats.h>
#include <xrpl/protocol/TxMeta.h>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

using namespace util;
using namespace data::cassandra;
using namespace migration;
using namespace util::config;

// Wire the production MPTTransactionHistoryMigrator against the production
// CassandraMigrationBackend (the test backend's Backend type would not satisfy the migrator's
// Backend alias).
using SupportedMigrators = migration::impl::MigratorsRegister<
    migration::cassandra::CassandraMigrationBackend,
    migration::cassandra::MPTTransactionHistoryMigrator>;
using TestManager = migration::impl::MigrationManagerBase<SupportedMigrators>;

namespace {

constexpr auto kMigratorName = migration::cassandra::MPTTransactionHistoryMigrator::kName;

// Accounts shared across fixtures (valid base58 r-addresses).
constexpr auto kIssuer = "rM2AGCCCRb373FRuD8wHyUwUsh2dV4BW5Q";
constexpr auto kHolder = "rK1EX542EgA9m948JrJRaEzwLVEhqWvnr9";
constexpr auto kHolder2 = "rnd1nHuzceyQDqnLH8urWNr4QBKt4v7WVk";
constexpr auto kObserver = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kUnknownAccount = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";

constexpr std::uint32_t kLedgerSeq = 100;
constexpr std::uint32_t kIssuanceSeq = 7;

// Minimal ledger header (seq is overwritten before writing) used to seed a valid ledger range.
constexpr auto kRawHeader =
    "03C3141A01633CD656F91B4EBB5EB89B791BD34DBC8A04BB6F407C5335BC54351E"
    "DD733898497E809E04074D14D271E4832D7888754F9230800761563A292FA2315A"
    "6DB6FE30CC5909B285080FCD6773CC883F9FE0EE4D439340AC592AADB973ED3CF5"
    "3E2232B33EF57CECAC2816E3122816E31A0A00F8377CD95DFA484CFAE282656A58"
    "CE5AA29652EFFD80AC59CD91416E4E13DBBE";

// An MPToken holder node carrying the issuance ID directly (the sfMPTokenIssuanceID path).
xrpl::STObject
createMPTokenNode(xrpl::uint192 const& issuanceID, std::string_view holder)
{
    xrpl::STObject fields(xrpl::sfFinalFields);
    fields.setAccountID(xrpl::sfAccount, getAccountIdWithString(holder));
    fields[xrpl::sfMPTokenIssuanceID] = issuanceID;

    xrpl::STObject node(xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN);
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{});
    node.set(std::move(fields));
    return node;
}

// An MPTokenIssuance node whose ID must be reconstructed from sfSequence + sfIssuer.
xrpl::STObject
createMPTokenIssuanceNode(std::uint32_t seq, std::string_view issuer)
{
    xrpl::STObject fields(xrpl::sfFinalFields);
    fields.setFieldU32(xrpl::sfSequence, seq);
    fields.setAccountID(xrpl::sfIssuer, getAccountIdWithString(issuer));

    xrpl::STObject node(xrpl::sfModifiedNode);
    node.setFieldU16(xrpl::sfLedgerEntryType, xrpl::ltMPTOKEN_ISSUANCE);
    node.setFieldH256(xrpl::sfLedgerIndex, xrpl::uint256{});
    node.set(std::move(fields));
    return node;
}

// A single Payment whose metadata touches two distinct issuances and three affected accounts,
// exercising the multi-issuance fan-out and per-account indexing.
std::pair<xrpl::STTx, xrpl::TxMeta>
makeMultiIssuancePayment(std::uint32_t ledgerSeq, std::uint32_t txIndex)
{
    xrpl::Slice const signingKey("test", 4);
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttPAYMENT);
    tx.setAccountID(xrpl::sfAccount, getAccountIdWithString(kHolder));
    tx.setFieldAmount(xrpl::sfAmount, xrpl::STAmount(100, false));
    tx.setFieldAmount(xrpl::sfFee, xrpl::STAmount(10, false));
    tx.setAccountID(xrpl::sfDestination, getAccountIdWithString(kHolder2));
    tx.setFieldU32(xrpl::sfSequence, 1);
    tx.setFieldVL(xrpl::sfSigningPubKey, signingKey);

    auto const serialized = tx.getSerializer();
    xrpl::STTx const sttx{xrpl::SerialIter{serialized.slice()}};

    auto const issuanceA = xrpl::makeMptID(1, getAccountIdWithString(kHolder));
    auto const issuanceB = xrpl::makeMptID(2, getAccountIdWithString(kHolder));

    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tesSUCCESS);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, txIndex);

    xrpl::STArray affectedNodes(xrpl::sfAffectedNodes);
    affectedNodes.push_back(createMPTokenNode(issuanceA, kHolder));
    affectedNodes.push_back(createMPTokenNode(issuanceB, kHolder2));
    affectedNodes.push_back(createMPTokenNode(issuanceA, kObserver));
    affectedNodes.push_back(createMPTokenIssuanceNode(1, kHolder));  // resolves to issuanceA
    metaObj.setFieldArray(xrpl::sfAffectedNodes, affectedNodes);

    xrpl::TxMeta const txMeta{
        sttx.getTransactionID(), ledgerSeq, metaObj.getSerializer().peekData()
    };
    return {sttx, txMeta};
}

std::string
blobToString(xrpl::Blob const& blob)
{
    return {reinterpret_cast<char const*>(blob.data()), blob.size()};
}

// A failed transaction with a top-level sfMPTokenIssuanceID exercises parity for records derived
// from the transaction body rather than successful metadata nodes.
std::pair<xrpl::STTx, xrpl::TxMeta>
makeFailedExplicitMPTReferenceTx(
    xrpl::uint192 const& issuanceID,
    std::uint32_t ledgerSeq,
    std::uint32_t txIndex
)
{
    xrpl::Slice const signingKey("test", 4);
    xrpl::STObject tx(xrpl::sfTransaction);
    tx.setFieldU16(xrpl::sfTransactionType, xrpl::ttMPTOKEN_ISSUANCE_SET);
    tx.setAccountID(xrpl::sfAccount, getAccountIdWithString(kIssuer));
    tx[xrpl::sfMPTokenIssuanceID] = issuanceID;
    tx.setFieldAmount(xrpl::sfFee, xrpl::STAmount(10, false));
    tx.setFieldU32(xrpl::sfSequence, 2);
    tx.setFieldVL(xrpl::sfSigningPubKey, signingKey);

    auto const serialized = tx.getSerializer();
    xrpl::STTx const sttx{xrpl::SerialIter{serialized.slice()}};

    xrpl::STObject metaObj(xrpl::sfTransactionMetaData);
    metaObj.setFieldU8(xrpl::sfTransactionResult, xrpl::tecINCOMPLETE);
    metaObj.setFieldU32(xrpl::sfTransactionIndex, txIndex);
    metaObj.setFieldArray(xrpl::sfAffectedNodes, xrpl::STArray{xrpl::sfAffectedNodes});

    xrpl::TxMeta const txMeta{
        sttx.getTransactionID(), ledgerSeq, metaObj.getSerializer().peekData()
    };
    return {sttx, txMeta};
}

template <class UInt>
std::string
uintToString(UInt const& value)
{
    return {reinterpret_cast<char const*>(value.data()), value.size()};
}

}  // namespace

class MPTTransactionHistoryMigratorTest : public util::prometheus::WithPrometheus {
public:
    MPTTransactionHistoryMigratorTest()
    {
        auto const cfg = cfg_.getObject("database.cassandra");
        backend_ = std::make_shared<migration::cassandra::CassandraMigrationBackend>(
            SettingsProvider{cfg}, cache_
        );
        manager_ = std::make_shared<TestManager>(backend_, cfg_.getObject("migration"));
    }

    ~MPTTransactionHistoryMigratorTest() override
    {
        Handle const handle{TestGlobals::instance().backendHost};
        EXPECT_TRUE(handle.connect());
        EXPECT_TRUE(handle.execute("DROP KEYSPACE " + TestGlobals::instance().backendKeyspace));
    }

protected:
    static constexpr auto kCassandra = "cassandra";

    ClioConfigDefinition cfg_{
        {{"database.type", ConfigValue{ConfigType::String}.defaultValue(kCassandra)},
         {"database.cassandra.contact_points",
          ConfigValue{ConfigType::String}.defaultValue(TestGlobals::instance().backendHost)},
         {"database.cassandra.keyspace",
          ConfigValue{ConfigType::String}.defaultValue(TestGlobals::instance().backendKeyspace)},
         {"database.cassandra.provider", ConfigValue{ConfigType::String}.defaultValue(kCassandra)},
         {"database.cassandra.replication_factor",
          ConfigValue{ConfigType::Integer}.defaultValue(1)},
         {"database.cassandra.connect_timeout", ConfigValue{ConfigType::Integer}.defaultValue(2)},
         {"database.cassandra.secure_connect_bundle", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.port",
          ConfigValue{ConfigType::Integer}.withConstraint(gValidatePort).optional()},
         {"database.cassandra.table_prefix", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.max_write_requests_outstanding",
          ConfigValue{ConfigType::Integer}.defaultValue(10'000).withConstraint(gValidateUint32)},
         {"database.cassandra.max_read_requests_outstanding",
          ConfigValue{ConfigType::Integer}.defaultValue(100'000).withConstraint(gValidateUint32)},
         {"database.cassandra.threads",
          ConfigValue{ConfigType::Integer}
              .defaultValue(static_cast<uint32_t>(std::thread::hardware_concurrency()))
              .withConstraint(gValidateUint32)},
         {"database.cassandra.core_connections_per_host",
          ConfigValue{ConfigType::Integer}.defaultValue(1).withConstraint(gValidateUint16)},
         {"database.cassandra.queue_size_io",
          ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateUint16)},
         {"database.cassandra.write_batch_size",
          ConfigValue{ConfigType::Integer}.defaultValue(20).withConstraint(gValidateUint16)},
         {"database.cassandra.request_timeout",
          ConfigValue{ConfigType::Integer}.optional().withConstraint(gValidateUint32)},
         {"database.cassandra.username", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.password", ConfigValue{ConfigType::String}.optional()},
         {"database.cassandra.certfile", ConfigValue{ConfigType::String}.optional()},
         {"migration.full_scan_threads",
          ConfigValue{ConfigType::Integer}.defaultValue(2).withConstraint(gValidateUint32)},
         {"migration.full_scan_jobs",
          ConfigValue{ConfigType::Integer}.defaultValue(4).withConstraint(gValidateUint32)},
         {"migration.cursors_per_job",
          ConfigValue{ConfigType::Integer}.defaultValue(100).withConstraint(gValidateUint32)}}
    };

    data::LedgerCache cache_;
    std::shared_ptr<migration::cassandra::CassandraMigrationBackend> backend_;
    std::shared_ptr<migration::MigrationManagerInterface> manager_;

    using SeqIdx = std::tuple<std::uint32_t, std::uint32_t>;
    using IssuanceRow = std::tuple<std::string, SeqIdx, std::string>;
    using AccountRow = std::tuple<std::string, std::string, SeqIdx, std::string>;

    struct RawIndexRows {
        std::set<IssuanceRow> issuance;
        std::set<AccountRow> account;

        bool
        operator==(RawIndexRows const&) const = default;
    };

    // Seed a single-ledger range so index fetches resolve a valid sequence range.
    void
    setupLedgerRange(std::uint32_t seq)
    {
        std::string rawHeaderBlob = hexStringToBinaryString(kRawHeader);
        xrpl::LedgerHeader lgrInfo = util::deserializeHeader(xrpl::makeSlice(rawHeaderBlob));
        lgrInfo.seq = seq;
        backend_->writeLedger(lgrInfo, std::move(rawHeaderBlob));
        backend_->writeSuccessor(
            uint256ToString(data::kFirstKey), lgrInfo.seq, uint256ToString(data::kLastKey)
        );
        ASSERT_TRUE(backend_->finishWrites(lgrInfo.seq));
    }

    // Write a (sttx, txMeta) pair into the transactions table at the given ledger sequence.
    void
    seedTransaction(xrpl::STTx const& sttx, xrpl::TxMeta const& txMeta, std::uint32_t seq)
    {
        backend_->writeTransaction(
            uint256ToString(sttx.getTransactionID()),
            seq,
            0,
            blobToString(sttx.getSerializer().peekData()),
            blobToString(txMeta.getAsObject().getSerializer().peekData())
        );
    }

    static etl::model::Transaction
    makeModelTx(xrpl::STTx const& sttx, xrpl::TxMeta const& txMeta)
    {
        auto raw = blobToString(sttx.getSerializer().peekData());
        auto metaRaw = blobToString(txMeta.getAsObject().getSerializer().peekData());
        auto const txID = sttx.getTransactionID();

        return etl::model::Transaction{
            .raw = std::move(raw),
            .metaRaw = std::move(metaRaw),
            .sttx = sttx,
            .meta = txMeta,
            .id = txID,
            .key = uintToString(txID),
            .type = sttx.getTxnType()
        };
    }

    static RawIndexRows
    expectedRowsFrom(std::vector<MPTokenIssuanceTransactionsData> const& expected)
    {
        RawIndexRows rows;
        for (auto const& rec : expected) {
            auto const issuanceID = uintToString(rec.mptIssuanceID);
            auto const txHash = uintToString(rec.txHash);
            auto const seqIdx = SeqIdx{rec.ledgerSequence, rec.transactionIndex};
            rows.issuance.emplace(issuanceID, seqIdx, txHash);

            for (auto const& account : rec.accounts)
                rows.account.emplace(issuanceID, uintToString(account), seqIdx, txHash);
        }

        return rows;
    }

    static void
    appendExpected(RawIndexRows& target, std::vector<MPTokenIssuanceTransactionsData> const& data)
    {
        auto rows = expectedRowsFrom(data);
        target.issuance.insert(rows.issuance.begin(), rows.issuance.end());
        target.account.insert(rows.account.begin(), rows.account.end());
    }

    RawIndexRows
    readRawIndexRows()
    {
        RawIndexRows rows;
        auto const cfg = cfg_.getObject("database.cassandra");
        auto const settings = SettingsProvider{cfg};
        Handle const handle{TestGlobals::instance().backendHost};
        auto const connected = handle.connect();
        EXPECT_TRUE(connected);
        if (not connected)
            return rows;

        auto const issuanceRes = handle.execute(
            "SELECT mptoken_issuance_id, seq_idx, hash FROM " +
            qualifiedTableName(settings, "mptoken_issuance_transactions")
        );
        EXPECT_TRUE(issuanceRes);
        if (issuanceRes) {
            for (auto const& [issuanceID, seqIdx, txHash] :
                 extract<std::vector<unsigned char>, SeqIdx, xrpl::uint256>(*issuanceRes)) {
                rows.issuance.emplace(blobToString(issuanceID), seqIdx, uintToString(txHash));
            }
        }

        auto const accountRes = handle.execute(
            "SELECT mptoken_issuance_id, account, seq_idx, hash FROM " +
            qualifiedTableName(settings, "account_mptoken_issuance_transactions")
        );
        EXPECT_TRUE(accountRes);
        if (accountRes) {
            for (auto const& [issuanceID, account, seqIdx, txHash] :
                 extract<std::vector<unsigned char>, xrpl::AccountID, SeqIdx, xrpl::uint256>(
                     *accountRes
                 )) {
                rows.account.emplace(
                    blobToString(issuanceID), uintToString(account), seqIdx, uintToString(txHash)
                );
            }
        }

        return rows;
    }

    void
    truncateIndexTables()
    {
        auto const cfg = cfg_.getObject("database.cassandra");
        auto const settings = SettingsProvider{cfg};
        Handle const handle{TestGlobals::instance().backendHost};
        ASSERT_TRUE(handle.connect());
        ASSERT_TRUE(handle.execute(
            "TRUNCATE " + qualifiedTableName(settings, "mptoken_issuance_transactions")
        ));
        ASSERT_TRUE(handle.execute(
            "TRUNCATE " + qualifiedTableName(settings, "account_mptoken_issuance_transactions")
        ));
    }

    void
    expectRawRowsEqual(RawIndexRows const& expected)
    {
        auto const actual = readRawIndexRows();
        EXPECT_EQ(actual.issuance, expected.issuance);
        EXPECT_EQ(actual.account, expected.account);
    }
};

// Status is NotMigrated until the migrator runs.
TEST_F(MPTTransactionHistoryMigratorTest, StatusNotMigratedBeforeRun)
{
    EXPECT_EQ(
        manager_->getMigratorStatusByName(kMigratorName), MigratorStatus::Status::NotMigrated
    );
}

// Backfill indexes the issuance-node path (MPTokenIssuanceCreate) and the multi-issuance fan-out
// (a Payment touching two issuances and three accounts). Asserts the index tables match exactly
// what the shared extractor produces, then that status flips to Migrated.
TEST_F(MPTTransactionHistoryMigratorTest, BackfillIndexesAllShapes)
{
    setupLedgerRange(kLedgerSeq);

    auto const createData = createMPTIssuanceCreateTxWithMetadata(kIssuer, 2, kIssuanceSeq);
    xrpl::STTx const createTx{
        xrpl::SerialIter{createData.transaction.data(), createData.transaction.size()}
    };
    xrpl::TxMeta const createMeta{createTx.getTransactionID(), kLedgerSeq, createData.metadata};
    seedTransaction(createTx, createMeta, kLedgerSeq);

    auto const [payTx, payMeta] = makeMultiIssuancePayment(kLedgerSeq, 1);
    seedTransaction(payTx, payMeta, kLedgerSeq);

    backend_->waitForWritesToFinish();

    auto const expectedCreate = etl::getMPTokenIssuanceTxsFromTx(createMeta, createTx);
    auto const expectedPay = etl::getMPTokenIssuanceTxsFromTx(payMeta, payTx);
    ASSERT_EQ(expectedCreate.size(), 1u);
    ASSERT_EQ(expectedPay.size(), 2u);
    for (auto const& rec : expectedPay) {
        ASSERT_EQ(rec.accounts.size(), 3u);
        EXPECT_TRUE(rec.accounts.contains(getAccountIdWithString(kHolder)));
        EXPECT_TRUE(rec.accounts.contains(getAccountIdWithString(kHolder2)));
        EXPECT_TRUE(rec.accounts.contains(getAccountIdWithString(kObserver)));
    }

    EXPECT_EQ(
        manager_->getMigratorStatusByName(kMigratorName), MigratorStatus::Status::NotMigrated
    );
    manager_->runMigration(kMigratorName);
    EXPECT_EQ(manager_->getMigratorStatusByName(kMigratorName), MigratorStatus::Status::Migrated);

    RawIndexRows expected;
    appendExpected(expected, expectedCreate);
    appendExpected(expected, expectedPay);
    expectRawRowsEqual(expected);

    // An account that never appears in any metadata has no rows.
    auto actual = readRawIndexRows();
    for (auto const& row : actual.account)
        EXPECT_NE(std::get<1>(row), uintToString(getAccountIdWithString(kUnknownAccount)));
}

// Rerunning the migrator rewrites identical deterministic-key rows, so exact rows are unchanged.
TEST_F(MPTTransactionHistoryMigratorTest, RerunIsIdempotent)
{
    setupLedgerRange(kLedgerSeq);

    auto const [payTx, payMeta] = makeMultiIssuancePayment(kLedgerSeq, 1);
    seedTransaction(payTx, payMeta, kLedgerSeq);
    backend_->waitForWritesToFinish();

    manager_->runMigration(kMigratorName);
    auto const afterFirst = readRawIndexRows();
    EXPECT_FALSE(afterFirst.issuance.empty());
    EXPECT_FALSE(afterFirst.account.empty());

    manager_->runMigration(kMigratorName);
    EXPECT_EQ(readRawIndexRows(), afterFirst);
    EXPECT_EQ(manager_->getMigratorStatusByName(kMigratorName), MigratorStatus::Status::Migrated);
}

// An empty transactions table runs cleanly and still reports Migrated.
TEST_F(MPTTransactionHistoryMigratorTest, EmptyTableRunsCleanly)
{
    setupLedgerRange(kLedgerSeq);

    manager_->runMigration(kMigratorName);
    EXPECT_EQ(manager_->getMigratorStatusByName(kMigratorName), MigratorStatus::Status::Migrated);
    expectRawRowsEqual({});
}

// Backfill and live ETL produce identical rows for the same transactions, including a failed
// transaction whose only MPT reference is in the transaction body.
TEST_F(MPTTransactionHistoryMigratorTest, BackfillMatchesLiveETLRows)
{
    setupLedgerRange(kLedgerSeq);

    auto const [payTx, payMeta] = makeMultiIssuancePayment(kLedgerSeq, 1);
    auto const [failedTx, failedMeta] = makeFailedExplicitMPTReferenceTx(
        xrpl::makeMptID(kIssuanceSeq, getAccountIdWithString(kIssuer)), kLedgerSeq, 2
    );

    seedTransaction(payTx, payMeta, kLedgerSeq);
    seedTransaction(failedTx, failedMeta, kLedgerSeq);
    backend_->waitForWritesToFinish();

    auto const expectedFailed =
        expectedRowsFrom(etl::getMPTokenIssuanceTxsFromTx(failedMeta, failedTx));
    ASSERT_EQ(expectedFailed.issuance.size(), 1u);

    etl::impl::MPTExt liveExt{backend_};
    liveExt.onLedgerData(
        etl::model::LedgerData{
            .transactions = {makeModelTx(payTx, payMeta), makeModelTx(failedTx, failedMeta)},
            .objects = {},
            .successors = {},
            .edgeKeys = {},
            .header = xrpl::LedgerHeader{},
            .rawHeader = {},
            .seq = kLedgerSeq
        }
    );
    backend_->waitForWritesToFinish();

    auto const liveRows = readRawIndexRows();
    EXPECT_FALSE(liveRows.issuance.empty());
    EXPECT_FALSE(liveRows.account.empty());
    EXPECT_TRUE(liveRows.issuance.contains(*expectedFailed.issuance.begin()));

    truncateIndexTables();
    expectRawRowsEqual({});

    manager_->runMigration(kMigratorName);
    EXPECT_EQ(readRawIndexRows(), liveRows);
}
