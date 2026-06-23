#include "data/BackendInterface.hpp"
#include "data/DBHelpers.hpp"
#include "data/LedgerCache.hpp"
#include "data/Types.hpp"
#include "data/cassandra/Handle.hpp"
#include "data/cassandra/SettingsProvider.hpp"
#include "etl/MPTHelpers.hpp"
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

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
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
ripple::STObject
createMPTokenNode(ripple::uint192 const& issuanceID, std::string_view holder)
{
    ripple::STObject fields(ripple::sfFinalFields);
    fields.setAccountID(ripple::sfAccount, getAccountIdWithString(holder));
    fields[ripple::sfMPTokenIssuanceID] = issuanceID;

    ripple::STObject node(ripple::sfModifiedNode);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltMPTOKEN);
    node.setFieldH256(ripple::sfLedgerIndex, ripple::uint256{});
    node.emplace_back(std::move(fields));
    return node;
}

// An MPTokenIssuance node whose ID must be reconstructed from sfSequence + sfIssuer.
ripple::STObject
createMPTokenIssuanceNode(std::uint32_t seq, std::string_view issuer)
{
    ripple::STObject fields(ripple::sfFinalFields);
    fields.setFieldU32(ripple::sfSequence, seq);
    fields.setAccountID(ripple::sfIssuer, getAccountIdWithString(issuer));

    ripple::STObject node(ripple::sfModifiedNode);
    node.setFieldU16(ripple::sfLedgerEntryType, ripple::ltMPTOKEN_ISSUANCE);
    node.setFieldH256(ripple::sfLedgerIndex, ripple::uint256{});
    node.emplace_back(std::move(fields));
    return node;
}

// A single Payment whose metadata touches two distinct issuances and three affected accounts,
// exercising the multi-issuance fan-out and per-account indexing.
std::pair<ripple::STTx, ripple::TxMeta>
makeMultiIssuancePayment(std::uint32_t ledgerSeq, std::uint32_t txIndex)
{
    ripple::Slice const signingKey("test", 4);
    ripple::STObject tx(ripple::sfTransaction);
    tx.setFieldU16(ripple::sfTransactionType, ripple::ttPAYMENT);
    tx.setAccountID(ripple::sfAccount, getAccountIdWithString(kHolder));
    tx.setFieldAmount(ripple::sfAmount, ripple::STAmount(100, false));
    tx.setFieldAmount(ripple::sfFee, ripple::STAmount(10, false));
    tx.setAccountID(ripple::sfDestination, getAccountIdWithString(kHolder2));
    tx.setFieldU32(ripple::sfSequence, 1);
    tx.setFieldVL(ripple::sfSigningPubKey, signingKey);

    auto const serialized = tx.getSerializer();
    ripple::STTx const sttx{ripple::SerialIter{serialized.slice()}};

    auto const issuanceA = ripple::makeMptID(1, getAccountIdWithString(kHolder));
    auto const issuanceB = ripple::makeMptID(2, getAccountIdWithString(kHolder));

    ripple::STObject metaObj(ripple::sfTransactionMetaData);
    metaObj.setFieldU8(ripple::sfTransactionResult, ripple::tesSUCCESS);
    metaObj.setFieldU32(ripple::sfTransactionIndex, txIndex);

    ripple::STArray affectedNodes(ripple::sfAffectedNodes);
    affectedNodes.push_back(createMPTokenNode(issuanceA, kHolder));
    affectedNodes.push_back(createMPTokenNode(issuanceB, kHolder2));
    affectedNodes.push_back(createMPTokenIssuanceNode(1, kHolder));  // resolves to issuanceA
    metaObj.setFieldArray(ripple::sfAffectedNodes, affectedNodes);

    ripple::TxMeta const txMeta{
        sttx.getTransactionID(), ledgerSeq, metaObj.getSerializer().peekData()
    };
    return {sttx, txMeta};
}

std::string
blobToString(ripple::Blob const& blob)
{
    return {reinterpret_cast<char const*>(blob.data()), blob.size()};
}

// Whether any returned transaction (re-fetched by hash) matches the given transaction ID.
bool
containsTx(std::vector<data::TransactionAndMetadata> const& txns, ripple::uint256 const& id)
{
    return std::ranges::any_of(txns, [&](auto const& tx) {
        if (tx.transaction.empty())
            return false;
        ripple::SerialIter it{tx.transaction.data(), tx.transaction.size()};
        return ripple::STTx{it}.getTransactionID() == id;
    });
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

    // Seed a single-ledger range so index fetches resolve a valid sequence range.
    void
    setupLedgerRange(std::uint32_t seq)
    {
        std::string rawHeaderBlob = hexStringToBinaryString(kRawHeader);
        ripple::LedgerHeader lgrInfo = util::deserializeHeader(ripple::makeSlice(rawHeaderBlob));
        lgrInfo.seq = seq;
        backend_->writeLedger(lgrInfo, std::move(rawHeaderBlob));
        backend_->writeSuccessor(
            uint256ToString(data::kFirstKey), lgrInfo.seq, uint256ToString(data::kLastKey)
        );
        ASSERT_TRUE(backend_->finishWrites(lgrInfo.seq));
    }

    // Write a (sttx, txMeta) pair into the transactions table at the given ledger sequence.
    void
    seedTransaction(ripple::STTx const& sttx, ripple::TxMeta const& txMeta, std::uint32_t seq)
    {
        backend_->writeTransaction(
            uint256ToString(sttx.getTransactionID()),
            seq,
            0,
            blobToString(sttx.getSerializer().peekData()),
            blobToString(txMeta.getAsObject().getSerializer().peekData())
        );
    }

    // Assert every record the extractor produces is present in both index tables.
    void
    expectIndexed(std::vector<MPTokenIssuanceTransactionsData> const& expected)
    {
        for (auto const& rec : expected) {
            auto const issuanceRes = data::synchronous([&](auto yield) {
                return backend_->fetchMPTokenIssuanceTransactions(
                    rec.mptIssuanceID, 1000, false, {}, yield
                );
            });
            EXPECT_TRUE(containsTx(issuanceRes.txns, rec.txHash))
                << "missing mptoken_issuance_transactions row for "
                << ripple::to_string(rec.mptIssuanceID);

            for (auto const& account : rec.accounts) {
                auto const accountRes = data::synchronous([&](auto yield) {
                    return backend_->fetchAccountMPTokenIssuanceTransactions(
                        rec.mptIssuanceID, account, 1000, false, {}, yield
                    );
                });
                EXPECT_TRUE(containsTx(accountRes.txns, rec.txHash))
                    << "missing account_mptoken_issuance_transactions row for "
                    << ripple::to_string(rec.mptIssuanceID);
            }
        }
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
    ripple::STTx const createTx{
        ripple::SerialIter{createData.transaction.data(), createData.transaction.size()}
    };
    ripple::TxMeta const createMeta{createTx.getTransactionID(), kLedgerSeq, createData.metadata};
    seedTransaction(createTx, createMeta, kLedgerSeq);

    auto const [payTx, payMeta] = makeMultiIssuancePayment(kLedgerSeq, 1);
    seedTransaction(payTx, payMeta, kLedgerSeq);

    backend_->waitForWritesToFinish();

    auto const expectedCreate = etl::getMPTokenIssuanceTxsFromTx(createMeta, createTx);
    auto const expectedPay = etl::getMPTokenIssuanceTxsFromTx(payMeta, payTx);
    ASSERT_EQ(expectedCreate.size(), 1u);
    ASSERT_EQ(expectedPay.size(), 2u);

    EXPECT_EQ(
        manager_->getMigratorStatusByName(kMigratorName), MigratorStatus::Status::NotMigrated
    );
    manager_->runMigration(kMigratorName);
    EXPECT_EQ(manager_->getMigratorStatusByName(kMigratorName), MigratorStatus::Status::Migrated);

    expectIndexed(expectedCreate);
    expectIndexed(expectedPay);

    // An account that never appears in any metadata has no rows.
    auto const unknown = data::synchronous([&](auto yield) {
        return backend_->fetchAccountMPTokenIssuanceTransactions(
            expectedPay.front().mptIssuanceID,
            getAccountIdWithString(kIssuer),
            1000,
            false,
            {},
            yield
        );
    });
    EXPECT_FALSE(containsTx(unknown.txns, payTx.getTransactionID()));
}

// Rerunning the migrator rewrites identical deterministic-key rows, so row counts are unchanged.
TEST_F(MPTTransactionHistoryMigratorTest, RerunIsIdempotent)
{
    setupLedgerRange(kLedgerSeq);

    auto const [payTx, payMeta] = makeMultiIssuancePayment(kLedgerSeq, 1);
    seedTransaction(payTx, payMeta, kLedgerSeq);
    backend_->waitForWritesToFinish();

    auto const issuanceA = ripple::makeMptID(1, getAccountIdWithString(kHolder));

    auto const countRows = [&]() {
        return data::synchronous(
                   [&](auto yield) {
                       return backend_->fetchMPTokenIssuanceTransactions(
                           issuanceA, 1000, false, {}, yield
                       );
                   }
        ).txns.size();
    };

    manager_->runMigration(kMigratorName);
    auto const afterFirst = countRows();
    EXPECT_GT(afterFirst, 0u);

    manager_->runMigration(kMigratorName);
    EXPECT_EQ(countRows(), afterFirst);
    EXPECT_EQ(manager_->getMigratorStatusByName(kMigratorName), MigratorStatus::Status::Migrated);
}

// An empty transactions table runs cleanly and still reports Migrated.
TEST_F(MPTTransactionHistoryMigratorTest, EmptyTableRunsCleanly)
{
    setupLedgerRange(kLedgerSeq);

    manager_->runMigration(kMigratorName);
    EXPECT_EQ(manager_->getMigratorStatusByName(kMigratorName), MigratorStatus::Status::Migrated);
}
