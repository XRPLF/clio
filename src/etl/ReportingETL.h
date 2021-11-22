#ifndef RIPPLE_APP_REPORTING_REPORTINGETL_H_INCLUDED
#define RIPPLE_APP_REPORTING_REPORTINGETL_H_INCLUDED

#include <ripple/ledger/ReadView.h>
#include <boost/algorithm/string.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/websocket.hpp>
#include <backend/BackendInterface.h>
#include <etl/ETLSource.h>
#include <subscriptions/SubscriptionManager.h>

#include "org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h"
#include <grpcpp/grpcpp.h>

#include <condition_variable>
#include <mutex>
#include <queue>

#include <chrono>

struct AccountTransactionsData;
class SubscriptionManager;

/**
 * This class is responsible for continuously extracting data from a
 * p2p node, and writing that data to the databases. Usually, multiple different
 * processes share access to the same network accessible databases, in which
 * case only one such process is performing ETL and writing to the database. The
 * other processes simply monitor the database for new ledgers, and publish
 * those ledgers to the various subscription streams. If a monitoring process
 * determines that the ETL writer has failed (no new ledgers written for some
 * time), the process will attempt to become the ETL writer. If there are
 * multiple monitoring processes that try to become the ETL writer at the same
 * time, one will win out, and the others will fall back to
 * monitoring/publishing. In this sense, this class dynamically transitions from
 * monitoring to writing and from writing to monitoring, based on the activity
 * of other processes running on different machines.
 */
class ReportingETL
{
private:
    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    std::shared_ptr<ETLLoadBalancer> loadBalancer_;
    std::optional<uint32_t> onlineDeleteInterval_;
    uint32_t extractorThreads_ = 1;

    std::thread worker_;
    boost::asio::io_context& ioContext_;

    /// Strand to ensure that ledgers are published in order.
    /// If ETL is started far behind the network, ledgers will be written and
    /// published very rapidly. Monitoring processes will publish ledgers as
    /// they are written. However, to publish a ledger, the monitoring process
    /// needs to read all of the transactions for that ledger from the database.
    /// Reading the transactions from the database requires network calls, which
    /// can be slow. It is imperative however that the monitoring processes keep
    /// up with the writer, else the monitoring processes will not be able to
    /// detect if the writer failed. Therefore, publishing each ledger (which
    /// includes reading all of the transactions from the database) is done from
    /// the application wide asio io_service, and a strand is used to ensure
    /// ledgers are published in order
    boost::asio::io_context::strand publishStrand_;

    /// Mechanism for communicating with ETL sources. ETLLoadBalancer wraps an
    /// arbitrary number of ETL sources and load balances ETL requests across
    /// those sources.

    /// Mechanism for detecting when the network has validated a new ledger.
    /// This class provides a way to wait for a specific ledger to be validated
    std::shared_ptr<NetworkValidatedLedgers> networkValidatedLedgers_;

    /// Whether the software is stopping
    std::atomic_bool stopping_ = false;
    /// Whether the software is performing online delete
    // TODO this needs to live in the database, so diff servers can coordinate
    // deletion
    std::atomic_bool deleting_ = false;

    /// Used to determine when to write to the database during the initial
    /// ledger download. By default, the software downloads an entire ledger and
    /// then writes to the database. If flushInterval_ is non-zero, the software
    /// will write to the database as new ledger data (SHAMap leaf nodes)
    /// arrives. It is not neccesarily more effient to write the data as it
    /// arrives, as different SHAMap leaf nodes share the same SHAMap inner
    /// nodes; flushing prematurely can result in the same SHAMap inner node
    /// being written to the database more than once. It is recommended to use
    /// the default value of 0 for this variable; however, different values can
    /// be experimented with if better performance is desired.
    size_t flushInterval_ = 0;

    /// This variable controls the number of GetLedgerData calls that will be
    /// executed in parallel during the initial ledger download. GetLedgerData
    /// allows clients to page through a ledger over many RPC calls.
    /// GetLedgerData returns a marker that is used as an offset in a subsequent
    /// call. If numMarkers_ is greater than 1, there will be multiple chains of
    /// GetLedgerData calls iterating over different parts of the same ledger in
    /// parallel. This can dramatically speed up the time to download the
    /// initial ledger. However, a higher value for this member variable puts
    /// more load on the ETL source.
    size_t numMarkers_ = 2;

    /// Whether the process is in strict read-only mode. In strict read-only
    /// mode, the process will never attempt to become the ETL writer, and will
    /// only publish ledgers as they are written to the database.
    bool readOnly_ = false;

    /// Whether the process is writing to the database. Used by server_info
    std::atomic_bool writing_ = false;

    /// Ledger sequence to start ETL from. If this is empty, ETL will start from
    /// the next ledger validated by the network. If this is set, and the
    /// database is already populated, an error is thrown.
    std::optional<uint32_t> startSequence_;
    std::optional<uint32_t> finishSequence_;

    size_t accumTxns_ = 0;
    size_t txnThreshold_ = 0;

    /// The time that the most recently published ledger was published. Used by
    /// server_info
    std::chrono::time_point<std::chrono::system_clock> lastPublish_;

    std::mutex publishTimeMtx_;

    std::chrono::time_point<std::chrono::system_clock>
    getLastPublish()
    {
        std::unique_lock<std::mutex> lck(publishTimeMtx_);
        return lastPublish_;
    }

    void
    setLastPublish()
    {
        std::unique_lock<std::mutex> lck(publishTimeMtx_);
        lastPublish_ = std::chrono::system_clock::now();
    }

    /// Download a ledger with specified sequence in full, via GetLedgerData,
    /// and write the data to the databases. This takes several minutes or
    /// longer.
    /// @param sequence the sequence of the ledger to download
    /// @return The ledger downloaded, with a full transaction and account state
    /// map
    std::optional<ripple::LedgerInfo>
    loadInitialLedger(uint32_t sequence);

    /// Run ETL. Extracts ledgers and writes them to the database, until a write
    /// conflict occurs (or the server shuts down).
    /// @note database must already be populated when this function is called
    /// @param startSequence the first ledger to extract
    /// @return the last ledger written to the database, if any
    std::optional<uint32_t>
    runETLPipeline(uint32_t startSequence, int offset);

    /// Monitor the network for newly validated ledgers. Also monitor the
    /// database to see if any process is writing those ledgers. This function
    /// is called when the application starts, and will only return when the
    /// application is shutting down. If the software detects the database is
    /// empty, this function will call loadInitialLedger(). If the software
    /// detects ledgers are not being written, this function calls
    /// runETLPipeline(). Otherwise, this function publishes ledgers as they are
    /// written to the database.
    void
    monitor();

    /// Monitor the database for newly written ledgers.
    /// Similar to the monitor(), except this function will never call
    /// runETLPipeline() or loadInitialLedger(). This function only publishes
    /// ledgers as they are written to the database.
    void
    monitorReadOnly();

    /// Extract data for a particular ledger from an ETL source. This function
    /// continously tries to extract the specified ledger (using all available
    /// ETL sources) until the extraction succeeds, or the server shuts down.
    /// @param sequence sequence of the ledger to extract
    /// @return ledger header and transaction+metadata blobs. Empty optional
    /// if the server is shutting down
    std::optional<org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedgerData(uint32_t sequence);

    /// Extract data for a particular ledger from an ETL source. This function
    /// continously tries to extract the specified ledger (using all available
    /// ETL sources) until the extraction succeeds, or the server shuts down.
    /// @param sequence sequence of the ledger to extract
    /// @return ledger header, transaction+metadata blobs, and all ledger
    /// objects created, modified or deleted between this ledger and the parent.
    /// Empty optional if the server is shutting down
    std::optional<org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedgerDataAndDiff(uint32_t sequence);

    /// Insert all of the extracted transactions into the ledger
    /// @param ledger ledger to insert transactions into
    /// @param data data extracted from an ETL source
    /// @return struct that contains the neccessary info to write to the
    /// transctions and account_transactions tables in Postgres (mostly
    /// transaction hashes, corresponding nodestore hashes and affected
    /// accounts)
    std::vector<AccountTransactionsData>
    insertTransactions(
        ripple::LedgerInfo const& ledger,
        org::xrpl::rpc::v1::GetLedgerResponse& data);

    // TODO update this documentation
    /// Build the next ledger using the previous ledger and the extracted data.
    /// This function calls insertTransactions()
    /// @note rawData should be data that corresponds to the ledger immediately
    /// following parent
    /// @param parent the previous ledger
    /// @param rawData data extracted from an ETL source
    /// @return the newly built ledger and data to write to Postgres
    std::pair<ripple::LedgerInfo, bool>
    buildNextLedger(org::xrpl::rpc::v1::GetLedgerResponse& rawData);

    /// Attempt to read the specified ledger from the database, and then publish
    /// that ledger to the ledgers stream.
    /// @param ledgerSequence the sequence of the ledger to publish
    /// @param maxAttempts the number of times to attempt to read the ledger
    /// from the database. 1 attempt per second
    /// @return whether the ledger was found in the database and published
    bool
    publishLedger(uint32_t ledgerSequence, std::optional<uint32_t> maxAttempts);

    /// Publish the passed in ledger
    /// @param ledger the ledger to publish
    void
    publishLedger(ripple::LedgerInfo const& lgrInfo);

    bool
    isStopping()
    {
        return stopping_;
    }

    /// Get the number of markers to use during the initial ledger download.
    /// This is equivelent to the degree of parallelism during the initial
    /// ledger download
    /// @return the number of markers
    uint32_t
    getNumMarkers()
    {
        return numMarkers_;
    }

    boost::json::object
    getInfo()
    {
        boost::json::object result;

        result["etl_sources"] = loadBalancer_->toJson();
        result["is_writer"] = writing_.load();
        result["read_only"] = readOnly_;
        auto last = getLastPublish();
        if (last.time_since_epoch().count() != 0)
            result["last_publish_time"] = std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now() - getLastPublish())
                    .count());

        return result;
    }

    /// start all of the necessary components and begin ETL
    void
    run()
    {
        BOOST_LOG_TRIVIAL(info) << "Starting reporting etl";
        stopping_ = false;

        doWork();
    }

    void
    doWork();

public:
    ReportingETL(
        boost::json::object const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        std::shared_ptr<NetworkValidatedLedgers> ledgers);

    static std::shared_ptr<ReportingETL>
    make_ReportingETL(
        boost::json::object const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<ETLLoadBalancer> balancer,
        std::shared_ptr<NetworkValidatedLedgers> ledgers)
    {
        auto etl = std::make_shared<ReportingETL>(
            config, ioc, backend, subscriptions, balancer, ledgers);

        etl->run();

        return etl;
    }

    ~ReportingETL()
    {
        BOOST_LOG_TRIVIAL(info) << "onStop called";
        BOOST_LOG_TRIVIAL(debug) << "Stopping Reporting ETL";
        stopping_ = true;

        if (worker_.joinable())
            worker_.join();

        BOOST_LOG_TRIVIAL(debug) << "Joined ReportingETL worker thread";
    }
};

#endif
