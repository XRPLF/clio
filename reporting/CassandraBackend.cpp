#include <reporting/CassandraBackend.h>
namespace Backend {
template <class T, class F>
void
processAsyncWriteResponse(T& requestParams, CassFuture* fut, F func)
{
    CassandraBackend const& backend = *requestParams.backend;
    auto rc = cass_future_error_code(fut);
    if (rc != CASS_OK)
    {
        BOOST_LOG_TRIVIAL(error)
            << "ERROR!!! Cassandra insert error: " << rc << ", "
            << cass_error_desc(rc) << ", retrying ";
        // exponential backoff with a max wait of 2^10 ms (about 1 second)
        auto wait = std::chrono::milliseconds(
            lround(std::pow(2, std::min(10u, requestParams.currentRetries))));
        ++requestParams.currentRetries;
        std::shared_ptr<boost::asio::steady_timer> timer =
            std::make_shared<boost::asio::steady_timer>(
                backend.getIOContext(),
                std::chrono::steady_clock::now() + wait);
        timer->async_wait([timer, &requestParams, &func](
                              const boost::system::error_code& error) {
            func(requestParams, true);
        });
    }
    else
    {
        backend.finishAsyncWrite();
        int remaining = --requestParams.refs;
        if (remaining == 0)
            delete &requestParams;
    }
}
// Process the result of an asynchronous write. Retry on error
// @param fut cassandra future associated with the write
// @param cbData struct that holds the request parameters
void
flatMapWriteCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteCallbackData*>(cbData);
    auto func = [](auto& params, bool retry) {
        params.backend->write(params, retry);
    };

    processAsyncWriteResponse(requestParams, fut, func);
}

void
flatMapWriteBookCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteCallbackData*>(cbData);
    auto func = [](auto& params, bool retry) {
        params.backend->writeBook(params, retry);
    };
    processAsyncWriteResponse(requestParams, fut, func);
}
/*

void
retryWriteKey(CassandraBackend::WriteCallbackData& requestParams, bool isRetry)
{
    auto const& backend = *requestParams.backend;
    if (requestParams.isDeleted)
        backend.writeDeletedKey(requestParams, true);
    else
        backend.writeKey(requestParams, true);
}

void
flatMapWriteKeyCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteCallbackData*>(cbData);
    processAsyncWriteResponse(requestParams, fut, retryWriteKey);
}

void
flatMapGetCreatedCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteCallbackData*>(cbData);
    CassandraBackend const& backend = *requestParams.backend;
    auto rc = cass_future_error_code(fut);
    if (rc != CASS_OK)
        BOOST_LOG_TRIVIAL(info) << __func__;
    {
        BOOST_LOG_TRIVIAL(error)
            << "ERROR!!! Cassandra insert error: " << rc << ", "
            << cass_error_desc(rc) << ", retrying ";
        // exponential backoff with a max wait of 2^10 ms (about 1 second)
        auto wait = std::chrono::milliseconds(
            lround(std::pow(2, std::min(10u, requestParams.currentRetries))));
        ++requestParams.currentRetries;
        std::shared_ptr<boost::asio::steady_timer> timer =
            std::make_shared<boost::asio::steady_timer>(
                backend.ioContext_, std::chrono::steady_clock::now() + wait);
        timer->async_wait([timer, &requestParams, &backend](
                              const boost::system::error_code& error) {
            backend.writeKey(requestParams, true);
        });
    }
    else
    {
        auto finish = [&backend]() {
            --(backend.numRequestsOutstanding_);

            backend.throttleCv_.notify_all();
            if (backend.numRequestsOutstanding_ == 0)
                backend.syncCv_.notify_all();
        };
        CassandraResult result{cass_future_get_result(fut)};

        if (!result)
        {
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch get row error : " << rc
                                     << ", " << cass_error_desc(rc);
            finish();
            return;
        }
        requestParams.createdSequence = result.getUInt32();
        backend.writeDeletedKey(requestParams, false);
    }
}
*/
void
flatMapWriteTransactionCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteTransactionCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteTransactionCallbackData*>(cbData);
    auto func = [](auto& params, bool retry) {
        params.backend->writeTransaction(params, retry);
    };
    processAsyncWriteResponse(requestParams, fut, func);
}
void
flatMapWriteAccountTxCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteAccountTxCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteAccountTxCallbackData*>(cbData);
    auto func = [](auto& params, bool retry) {
        params.backend->writeAccountTx(params, retry);
    };
    processAsyncWriteResponse(requestParams, fut, func);
}
void
flatMapWriteLedgerHeaderCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteLedgerHeaderCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteLedgerHeaderCallbackData*>(cbData);
    auto func = [](auto& params, bool retry) {
        params.backend->writeLedgerHeader(params, retry);
    };
    processAsyncWriteResponse(requestParams, fut, func);
}

void
flatMapWriteLedgerHashCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteLedgerHashCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteLedgerHashCallbackData*>(cbData);
    auto func = [](auto& params, bool retry) {
        params.backend->writeLedgerHash(params, retry);
    };
    processAsyncWriteResponse(requestParams, fut, func);
}

// Process the result of an asynchronous read. Retry on error
// @param fut cassandra future associated with the read
// @param cbData struct that holds the request parameters
void
flatMapReadCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::ReadCallbackData& requestParams =
        *static_cast<CassandraBackend::ReadCallbackData*>(cbData);
    auto func = [](auto& params) { params.backend.read(params); };
    CassandraAsyncResult asyncResult{requestParams, fut, func};
    CassandraResult& result = asyncResult.getResult();

    if (!!result)
    {
        requestParams.result = {
            result.getBytes(), result.getBytes(), result.getUInt32()};
    }
}

// Process the result of an asynchronous read. Retry on error
// @param fut cassandra future associated with the read
// @param cbData struct that holds the request parameters
void
flatMapReadObjectCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::ReadObjectCallbackData& requestParams =
        *static_cast<CassandraBackend::ReadObjectCallbackData*>(cbData);
    auto func = [](auto& params) { params.backend.readObject(params); };
    CassandraAsyncResult asyncResult{requestParams, fut, func};
    CassandraResult& result = asyncResult.getResult();

    if (!!result)
    {
        requestParams.result = result.getBytes();
    }
}

std::optional<LedgerRange>
CassandraBackend::fetchLedgerRange() const
{
    BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
    CassandraStatement statement{selectLedgerRange_};
    CassandraResult result = executeSyncRead(statement);
    if (!result)
    {
        BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
        return {};
    }
    LedgerRange range;
    range.maxSequence = range.minSequence = result.getUInt32();
    if (result.nextRow())
    {
        range.maxSequence = result.getUInt32();
    }
    if (range.minSequence > range.maxSequence)
    {
        std::swap(range.minSequence, range.maxSequence);
    }
    return range;
}
std::vector<TransactionAndMetadata>
CassandraBackend::fetchAllTransactionsInLedger(uint32_t ledgerSequence) const
{
    CassandraStatement statement{selectAllTransactionsInLedger_};
    statement.bindInt(ledgerSequence);
    CassandraResult result = executeSyncRead(statement);
    if (!result)
    {
        BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
        return {};
    }
    std::vector<TransactionAndMetadata> txns;
    do
    {
        txns.push_back(
            {result.getBytes(), result.getBytes(), result.getUInt32()});
    } while (result.nextRow());
    return txns;
}
std::vector<ripple::uint256>
CassandraBackend::fetchAllTransactionHashesInLedger(
    uint32_t ledgerSequence) const
{
    CassandraStatement statement{selectAllTransactionHashesInLedger_};
    statement.bindInt(ledgerSequence);
    CassandraResult result = executeSyncRead(statement);
    if (!result)
    {
        BOOST_LOG_TRIVIAL(error) << __func__ << " - no rows";
        return {};
    }
    std::vector<ripple::uint256> hashes;
    do
    {
        hashes.push_back(result.getUInt256());
    } while (result.nextRow());
    return hashes;
}
bool
CassandraBackend::doOnlineDelete(uint32_t minLedgerToKeep) const
{
    throw std::runtime_error("doOnlineDelete : unimplemented");
    return false;
}

void
CassandraBackend::open(bool readOnly)
{
    std::cout << config_ << std::endl;
    auto getString = [this](std::string const& field) -> std::string {
        if (config_.contains(field))
        {
            auto jsonStr = config_[field].as_string();
            return {jsonStr.c_str(), jsonStr.size()};
        }
        return {""};
    };
    if (open_)
    {
        assert(false);
        BOOST_LOG_TRIVIAL(error) << "database is already open";
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    CassCluster* cluster = cass_cluster_new();
    if (!cluster)
        throw std::runtime_error("nodestore:: Failed to create CassCluster");

    std::string secureConnectBundle = getString("secure_connect_bundle");

    if (!secureConnectBundle.empty())
    {
        /* Setup driver to connect to the cloud using the secure connection
         * bundle */
        if (cass_cluster_set_cloud_secure_connection_bundle(
                cluster, secureConnectBundle.c_str()) != CASS_OK)
        {
            BOOST_LOG_TRIVIAL(error) << "Unable to configure cloud using the "
                                        "secure connection bundle: "
                                     << secureConnectBundle;
            throw std::runtime_error(
                "nodestore: Failed to connect using secure connection "
                "bundle");
            return;
        }
    }
    else
    {
        std::string contact_points = getString("contact_points");
        if (contact_points.empty())
        {
            throw std::runtime_error(
                "nodestore: Missing contact_points in Cassandra config");
        }
        CassError rc =
            cass_cluster_set_contact_points(cluster, contact_points.c_str());
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "nodestore: Error setting Cassandra contact_points: "
               << contact_points << ", result: " << rc << ", "
               << cass_error_desc(rc);

            throw std::runtime_error(ss.str());
        }

        int port = config_.contains("port") ? config_["port"].as_int64() : 0;
        if (port)
        {
            rc = cass_cluster_set_port(cluster, port);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "nodestore: Error setting Cassandra port: " << port
                   << ", result: " << rc << ", " << cass_error_desc(rc);

                throw std::runtime_error(ss.str());
            }
        }
    }
    cass_cluster_set_token_aware_routing(cluster, cass_true);
    CassError rc =
        cass_cluster_set_protocol_version(cluster, CASS_PROTOCOL_VERSION_V4);
    if (rc != CASS_OK)
    {
        std::stringstream ss;
        ss << "nodestore: Error setting cassandra protocol version: "
           << ", result: " << rc << ", " << cass_error_desc(rc);

        throw std::runtime_error(ss.str());
    }

    std::string username = getString("username");
    if (username.size())
    {
        BOOST_LOG_TRIVIAL(debug)
            << "user = " << username.c_str()
            << " password = " << getString("password").c_str();
        cass_cluster_set_credentials(
            cluster, username.c_str(), getString("password").c_str());
    }
    int threads = config_.contains("threads")
        ? config_["threads"].as_int64()
        : std::thread::hardware_concurrency();

    rc = cass_cluster_set_num_threads_io(cluster, threads);
    if (rc != CASS_OK)
    {
        std::stringstream ss;
        ss << "nodestore: Error setting Cassandra io threads to " << threads
           << ", result: " << rc << ", " << cass_error_desc(rc);
        throw std::runtime_error(ss.str());
    }

    cass_cluster_set_request_timeout(cluster, 10000);

    rc = cass_cluster_set_queue_size_io(
        cluster,
        maxRequestsOutstanding);  // This number needs to scale w/ the
                                  // number of request per sec
    if (rc != CASS_OK)
    {
        std::stringstream ss;
        ss << "nodestore: Error setting Cassandra max core connections per "
              "host"
           << ", result: " << rc << ", " << cass_error_desc(rc);
        BOOST_LOG_TRIVIAL(error) << ss.str();
        throw std::runtime_error(ss.str());
    }

    std::string certfile = getString("certfile");
    if (certfile.size())
    {
        std::ifstream fileStream(
            boost::filesystem::path(certfile).string(), std::ios::in);
        if (!fileStream)
        {
            std::stringstream ss;
            ss << "opening config file " << certfile;
            throw std::system_error(errno, std::generic_category(), ss.str());
        }
        std::string cert(
            std::istreambuf_iterator<char>{fileStream},
            std::istreambuf_iterator<char>{});
        if (fileStream.bad())
        {
            std::stringstream ss;
            ss << "reading config file " << certfile;
            throw std::system_error(errno, std::generic_category(), ss.str());
        }

        CassSsl* context = cass_ssl_new();
        cass_ssl_set_verify_flags(context, CASS_SSL_VERIFY_NONE);
        rc = cass_ssl_add_trusted_cert(context, cert.c_str());
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "nodestore: Error setting Cassandra ssl context: " << rc
               << ", " << cass_error_desc(rc);
            throw std::runtime_error(ss.str());
        }

        cass_cluster_set_ssl(cluster, context);
        cass_ssl_free(context);
    }

    std::string keyspace = getString("keyspace");
    if (keyspace.empty())
    {
        throw std::runtime_error(
            "nodestore: Missing keyspace in Cassandra config");
    }

    std::string tablePrefix = getString("table_prefix");
    if (tablePrefix.empty())
    {
        throw std::runtime_error(
            "nodestore: Missing table name in Cassandra config");
    }

    cass_cluster_set_connect_timeout(cluster, 10000);

    auto executeSimpleStatement = [this](std::string const& query) {
        CassStatement* statement = makeStatement(query.c_str(), 0);
        CassFuture* fut = cass_session_execute(session_.get(), statement);
        CassError rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK && rc != CASS_ERROR_SERVER_INVALID_QUERY)
        {
            std::stringstream ss;
            ss << "nodestore: Error executing simple statement: " << rc << ", "
               << cass_error_desc(rc) << " - " << query;
            BOOST_LOG_TRIVIAL(error) << ss.str();
            return false;
        }
        return true;
    };
    CassStatement* statement;
    CassFuture* fut;
    bool setupSessionAndTable = false;
    while (!setupSessionAndTable)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        session_.reset(cass_session_new());
        assert(session_);

        fut = cass_session_connect_keyspace(
            session_.get(), cluster, keyspace.c_str());
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "nodestore: Error connecting Cassandra session keyspace: "
               << rc << ", " << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        std::stringstream query;
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "objects"
              << " ( key blob, sequence bigint, object blob, PRIMARY "
                 "KEY(key, "
                 "sequence)) WITH CLUSTERING ORDER BY (sequence DESC)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "objects"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE INDEX ON " << tablePrefix << "objects(sequence)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "objects WHERE sequence=1"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query
            << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "transactions"
            << " ( hash blob PRIMARY KEY, ledger_sequence bigint, transaction "
               "blob, metadata blob)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "transactions"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE INDEX ON " << tablePrefix
              << "transactions(ledger_sequence)";
        if (!executeSimpleStatement(query.str()))
            continue;
        query = {};
        query << "SELECT * FROM " << tablePrefix
              << "transactions WHERE ledger_sequence = 1"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "books"
              << " ( book blob, sequence bigint, key blob, deleted_at "
                 "bigint, PRIMARY KEY "
                 "(book, key)) WITH CLUSTERING ORDER BY (key ASC)";
        if (!executeSimpleStatement(query.str()))
            continue;
        query = {};
        query << "SELECT * FROM " << tablePrefix << "books"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;
        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "account_tx"
              << " ( account blob, seq_idx tuple<bigint, bigint>, "
                 " hash blob, "
                 "PRIMARY KEY "
                 "(account, seq_idx)) WITH "
                 "CLUSTERING ORDER BY (seq_idx desc)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "account_tx"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "ledgers"
              << " ( sequence bigint PRIMARY KEY, header blob )";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "ledgers"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "ledger_hashes"
              << " (hash blob PRIMARY KEY, sequence bigint)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "ledger_hashes"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tablePrefix << "ledger_range"
              << " (is_latest boolean PRIMARY KEY, sequence bigint)";
        if (!executeSimpleStatement(query.str()))
            continue;

        query = {};
        query << "SELECT * FROM " << tablePrefix << "ledger_range"
              << " LIMIT 1";
        if (!executeSimpleStatement(query.str()))
            continue;
        setupSessionAndTable = true;
    }

    cass_cluster_free(cluster);

    bool setupPreparedStatements = false;
    while (!setupPreparedStatements)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::stringstream query;
        query << "INSERT INTO " << tablePrefix << "objects"
              << " (key, sequence, object) VALUES (?, ?, ?)";
        if (!insertObject_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query
            << "INSERT INTO " << tablePrefix << "transactions"
            << " (hash, ledger_sequence, transaction, metadata) VALUES (?, ?, "
               "?, ?)";
        if (!insertTransaction_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "INSERT INTO " << tablePrefix << "books"
              << " (book, key, sequence, deleted_at) VALUES (?, ?, ?, ?)";
        if (!insertBook_.prepareStatement(query, session_.get()))
            continue;
        query = {};
        query << "INSERT INTO " << tablePrefix << "books"
              << " (book, key, deleted_at) VALUES (?, ?, ?)";
        if (!deleteBook_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "SELECT object, sequence FROM " << tablePrefix << "objects"
              << " WHERE key = ? AND sequence <= ? ORDER BY sequence DESC "
                 "LIMIT 1";

        if (!selectObject_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "SELECT transaction, metadata, ledger_sequence FROM "
              << tablePrefix << "transactions"
              << " WHERE hash = ?";
        if (!selectTransaction_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "SELECT transaction, metadata, ledger_sequence FROM "
              << tablePrefix << "transactions"
              << " WHERE ledger_sequence = ?";
        if (!selectAllTransactionsInLedger_.prepareStatement(
                query, session_.get()))
            continue;
        query = {};
        query << "SELECT hash FROM " << tablePrefix << "transactions"
              << " WHERE ledger_sequence = ?";
        if (!selectAllTransactionHashesInLedger_.prepareStatement(
                query, session_.get()))
            continue;

        query = {};
        query << "SELECT object,key FROM " << tablePrefix << "objects "
              << " WHERE TOKEN(key) >= ? and sequence <= ? "
              << " PER PARTITION LIMIT 1 LIMIT ? ALLOW FILTERING";

        if (!selectLedgerPage_.prepareStatement(query, session_.get()))
            continue;

        /*
        query = {};
        query << "SELECT filterempty(key,object) FROM " << tablePrefix <<
        "objects "
              << " WHERE TOKEN(key) >= ? and sequence <= ?"
              << " PER PARTITION LIMIT 1 LIMIT ?"
              << " ALLOW FILTERING";
        if (!upperBound2_.prepareStatement(query, session_.get()))
            continue;
*/
        query = {};
        query << "SELECT TOKEN(key) FROM " << tablePrefix << "objects "
              << " WHERE key = ? LIMIT 1";

        if (!getToken_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << "SELECT key FROM " << tablePrefix << "books "
              << " WHERE book = ? AND sequence <= ? AND deleted_at > ? AND"
                 " key > ? "
                 " ORDER BY key ASC LIMIT ? ALLOW FILTERING";
        if (!getBook_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " INSERT INTO " << tablePrefix << "account_tx"
              << " (account, seq_idx, hash) "
              << " VALUES (?,?,?)";
        if (!insertAccountTx_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " SELECT hash,seq_idx FROM " << tablePrefix << "account_tx"
              << " WHERE account = ? "
              << " AND seq_idx < ? LIMIT ?";
        if (!selectAccountTx_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " INSERT INTO " << tablePrefix << "ledgers "
              << " (sequence, header) VALUES(?,?)";
        if (!insertLedgerHeader_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " INSERT INTO " << tablePrefix << "ledger_hashes"
              << " (hash, sequence) VALUES(?,?)";
        if (!insertLedgerHash_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query
            << " update " << tablePrefix << "ledger_range"
            << " set sequence = ? where is_latest = ? if sequence in (?,null)";
        if (!updateLedgerRange_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " select header from " << tablePrefix
              << "ledgers where sequence = ?";
        if (!selectLedgerBySeq_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " select sequence from " << tablePrefix
              << "ledger_range where is_latest = true";
        if (!selectLatestLedger_.prepareStatement(query, session_.get()))
            continue;

        query = {};
        query << " SELECT sequence FROM " << tablePrefix
              << "ledger_range WHERE "
              << " is_latest IN (true, false)";
        if (!selectLedgerRange_.prepareStatement(query, session_.get()))
            continue;

        setupPreparedStatements = true;
    }

    /*
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!fetchLatestLedgerSequence())
        {
            std::stringstream query;
            query << "TRUNCATE TABLE " << tablePrefix << "ledger_range";
            if (!executeSimpleStatement(query.str()))
                continue;
            query = {};
            query << "TRUNCATE TABLE " << tablePrefix << "ledgers";
            if (!executeSimpleStatement(query.str()))
                continue;
            query = {};
            query << "TRUNCATE TABLE " << tablePrefix << "ledger_hashes";
            if (!executeSimpleStatement(query.str()))
                continue;
            query = {};
            query << "TRUNCATE TABLE " << tablePrefix << "objects";
            if (!executeSimpleStatement(query.str()))
                continue;
            query = {};
            query << "TRUNCATE TABLE " << tablePrefix << "transactions";
            if (!executeSimpleStatement(query.str()))
                continue;
            query = {};
            query << "TRUNCATE TABLE " << tablePrefix << "account_tx";
            if (!executeSimpleStatement(query.str()))
                continue;
            query = {};
            query << "TRUNCATE TABLE " << tablePrefix << "books";
            if (!executeSimpleStatement(query.str()))
                continue;
        }
        break;
    }
    */

    if (config_.contains("max_requests_outstanding"))
    {
        maxRequestsOutstanding = config_["max_requests_outstanding"].as_int64();
    }
    work_.emplace(ioContext_);
    ioThread_ = std::thread{[this]() { ioContext_.run(); }};
    open_ = true;

    BOOST_LOG_TRIVIAL(info) << "Opened database successfully";
}
}  // namespace Backend
