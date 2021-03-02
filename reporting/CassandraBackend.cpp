#include <reporting/CassandraBackend.h>
namespace Backend {
// Process the result of an asynchronous write. Retry on error
// @param fut cassandra future associated with the write
// @param cbData struct that holds the request parameters
void
flatMapWriteCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteCallbackData*>(cbData);
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
                backend.ioContext_, std::chrono::steady_clock::now() + wait);
        timer->async_wait([timer, &requestParams, &backend](
                              const boost::system::error_code& error) {
            backend.write(requestParams, true);
        });
    }
    else
    {
        --(backend.numRequestsOutstanding_);

        backend.throttleCv_.notify_all();
        if (backend.numRequestsOutstanding_ == 0)
            backend.syncCv_.notify_all();
        int remaining = --requestParams.refs;
        if (remaining == 0)
            delete &requestParams;
    }
}
void
flatMapWriteBookCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteCallbackData*>(cbData);
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
                backend.ioContext_, std::chrono::steady_clock::now() + wait);
        timer->async_wait([timer, &requestParams, &backend](
                              const boost::system::error_code& error) {
            backend.writeBook(requestParams, true);
        });
    }
    else
    {
        --(backend.numRequestsOutstanding_);

        backend.throttleCv_.notify_all();
        if (backend.numRequestsOutstanding_ == 0)
            backend.syncCv_.notify_all();
        int remaining = --requestParams.refs;
        if (remaining == 0)
            delete &requestParams;
    }
}

void
flatMapWriteKeyCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteCallbackData*>(cbData);
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
                backend.ioContext_, std::chrono::steady_clock::now() + wait);
        timer->async_wait([timer, &requestParams, &backend](
                              const boost::system::error_code& error) {
            if (requestParams.isDeleted)
                backend.writeDeletedKey(requestParams, true);
            else
                backend.writeKey(requestParams, true);
        });
    }
    else
    {
        --(backend.numRequestsOutstanding_);

        backend.throttleCv_.notify_all();
        if (backend.numRequestsOutstanding_ == 0)
            backend.syncCv_.notify_all();
        int remaining = --requestParams.refs;
        if (remaining == 0)
            delete &requestParams;
    }
}
void
flatMapGetCreatedCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteCallbackData*>(cbData);
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
        CassResult const* res = cass_future_get_result(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch get row error : " << rc
                                     << ", " << cass_error_desc(rc);
            finish();
            return;
        }
        cass_int64_t created;
        rc = cass_value_get_int64(cass_row_get_column(row, 0), &created);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error)
                << "Cassandra fetch get bytes error : " << rc << ", "
                << cass_error_desc(rc);
            finish();
            return;
        }
        cass_result_free(res);
        requestParams.createdSequence = created;
        backend.writeDeletedKey(requestParams, false);
    }
}
void
flatMapWriteTransactionCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteTransactionCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteTransactionCallbackData*>(cbData);
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
                backend.ioContext_, std::chrono::steady_clock::now() + wait);
        timer->async_wait([timer, &requestParams, &backend](
                              const boost::system::error_code& error) {
            backend.writeTransaction(requestParams, true);
        });
    }
    else
    {
        --(backend.numRequestsOutstanding_);

        backend.throttleCv_.notify_all();
        if (backend.numRequestsOutstanding_ == 0)
            backend.syncCv_.notify_all();
        delete &requestParams;
    }
}
void
flatMapWriteAccountTxCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteAccountTxCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteAccountTxCallbackData*>(cbData);
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
                backend.ioContext_, std::chrono::steady_clock::now() + wait);
        timer->async_wait([timer, &requestParams, &backend](
                              const boost::system::error_code& error) {
            backend.writeAccountTx(requestParams, true);
        });
    }
    else
    {
        --(backend.numRequestsOutstanding_);

        backend.throttleCv_.notify_all();
        if (backend.numRequestsOutstanding_ == 0)
            backend.syncCv_.notify_all();
        int remaining = --requestParams.refs;
        if (remaining == 0)
            delete &requestParams;
    }
}
void
flatMapWriteLedgerHeaderCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteLedgerHeaderCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteLedgerHeaderCallbackData*>(cbData);
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
                backend.ioContext_, std::chrono::steady_clock::now() + wait);
        timer->async_wait([timer, &requestParams, &backend](
                              const boost::system::error_code& error) {
            backend.writeLedgerHeader(requestParams, true);
        });
    }
    else
    {
        --(backend.numRequestsOutstanding_);

        backend.throttleCv_.notify_all();
        if (backend.numRequestsOutstanding_ == 0)
            backend.syncCv_.notify_all();
        delete &requestParams;
    }
}
void
flatMapWriteLedgerHashCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::WriteLedgerHashCallbackData& requestParams =
        *static_cast<CassandraBackend::WriteLedgerHashCallbackData*>(cbData);
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
                backend.ioContext_, std::chrono::steady_clock::now() + wait);
        timer->async_wait([timer, &requestParams, &backend](
                              const boost::system::error_code& error) {
            backend.writeLedgerHash(requestParams, true);
        });
    }
    else
    {
        --(backend.numRequestsOutstanding_);

        backend.throttleCv_.notify_all();
        if (backend.numRequestsOutstanding_ == 0)
            backend.syncCv_.notify_all();
        delete &requestParams;
    }
}

// Process the result of an asynchronous read. Retry on error
// @param fut cassandra future associated with the read
// @param cbData struct that holds the request parameters
void
flatMapReadCallback(CassFuture* fut, void* cbData)
{
    CassandraBackend::ReadCallbackData& requestParams =
        *static_cast<CassandraBackend::ReadCallbackData*>(cbData);

    CassError rc = cass_future_error_code(fut);

    if (rc != CASS_OK)
    {
        BOOST_LOG_TRIVIAL(warning) << "Cassandra fetch error : " << rc << " : "
                                   << cass_error_desc(rc) << " - retrying";
        // Retry right away. The only time the cluster should ever be overloaded
        // is when the very first ledger is being written in full (millions of
        // writes at once), during which no reads should be occurring. If reads
        // are timing out, the code/architecture should be modified to handle
        // greater read load, as opposed to just exponential backoff
        requestParams.backend.read(requestParams);
    }
    else
    {
        auto finish = [&requestParams]() {
            size_t batchSize = requestParams.batchSize;
            if (++(requestParams.numFinished) == batchSize)
                requestParams.cv.notify_all();
        };
        CassResult const* res = cass_future_get_result(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch get row error : " << rc
                                     << ", " << cass_error_desc(rc);
            finish();
            return;
        }
        cass_byte_t const* buf;
        std::size_t bufSize;
        rc = cass_value_get_bytes(cass_row_get_column(row, 0), &buf, &bufSize);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error)
                << "Cassandra fetch get bytes error : " << rc << ", "
                << cass_error_desc(rc);
            finish();
            return;
        }

        std::vector<unsigned char> txn{buf, buf + bufSize};
        cass_byte_t const* buf2;
        std::size_t buf2Size;
        rc =
            cass_value_get_bytes(cass_row_get_column(row, 1), &buf2, &buf2Size);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error)
                << "Cassandra fetch get bytes error : " << rc << ", "
                << cass_error_desc(rc);
            finish();
            return;
        }
        std::vector<unsigned char> meta{buf2, buf2 + buf2Size};
        requestParams.result = {std::move(txn), std::move(meta)};
        cass_result_free(res);
        finish();
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

    CassError rc = cass_future_error_code(fut);

    if (rc != CASS_OK)
    {
        BOOST_LOG_TRIVIAL(warning) << "Cassandra fetch error : " << rc << " : "
                                   << cass_error_desc(rc) << " - retrying";
        // Retry right away. The only time the cluster should ever be overloaded
        // is when the very first ledger is being written in full (millions of
        // writes at once), during which no reads should be occurring. If reads
        // are timing out, the code/architecture should be modified to handle
        // greater read load, as opposed to just exponential backoff
        requestParams.backend.readObject(requestParams);
    }
    else
    {
        auto finish = [&requestParams]() {
            BOOST_LOG_TRIVIAL(trace)
                << "flatMapReadObjectCallback - finished a read";
            size_t batchSize = requestParams.batchSize;
            if (++(requestParams.numFinished) == batchSize)
                requestParams.cv.notify_all();
        };
        CassResult const* res = cass_future_get_result(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error)
                << "Cassandra fetch get row error : " << rc << ", "
                << cass_error_desc(rc)
                << " key = " << ripple::strHex(requestParams.key);
            finish();
            return;
        }
        cass_byte_t const* buf;
        std::size_t bufSize;
        rc = cass_value_get_bytes(cass_row_get_column(row, 0), &buf, &bufSize);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error)
                << "Cassandra fetch get bytes error : " << rc << ", "
                << cass_error_desc(rc);
            finish();
            return;
        }

        std::vector<unsigned char> obj{buf, buf + bufSize};
        requestParams.result = std::move(obj);
        cass_result_free(res);
        finish();
    }
}

void
CassandraBackend::open()
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

    unsigned int const workers = std::thread::hardware_concurrency();
    rc = cass_cluster_set_num_threads_io(cluster, workers);
    if (rc != CASS_OK)
    {
        std::stringstream ss;
        ss << "nodestore: Error setting Cassandra io threads to " << workers
           << ", result: " << rc << ", " << cass_error_desc(rc);
        throw std::runtime_error(ss.str());
    }

    cass_cluster_set_request_timeout(cluster, 2000);

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

    std::string tableName = getString("table_name");
    if (tableName.empty())
    {
        throw std::runtime_error(
            "nodestore: Missing table name in Cassandra config");
    }

    cass_cluster_set_connect_timeout(cluster, 10000);

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
        query << "CREATE TABLE IF NOT EXISTS " << tableName << "flat"
              << " ( key blob, sequence bigint, object blob, PRIMARY "
                 "KEY(key, "
                 "sequence)) WITH CLUSTERING ORDER BY (sequence DESC)";

        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK && rc != CASS_ERROR_SERVER_INVALID_QUERY)
        {
            std::stringstream ss;
            ss << "nodestore: Error creating Cassandra table: " << rc << ", "
               << cass_error_desc(rc) << " - " << query.str();
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        query = {};
        query << "SELECT * FROM " << tableName << "flat"
              << " LIMIT 1";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK)
        {
            if (rc == CASS_ERROR_SERVER_INVALID_QUERY)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << "table not here yet, sleeping 1s to "
                       "see if table creation propagates";
                continue;
            }
            else
            {
                std::stringstream ss;
                ss << "nodestore: Error checking for table: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << ss.str();
                continue;
            }
        }
        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tableName
              << "flattransactions"
              << " ( hash blob PRIMARY KEY, sequence bigint, transaction "
                 "blob, metadata blob)";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK && rc != CASS_ERROR_SERVER_INVALID_QUERY)
        {
            std::stringstream ss;
            ss << "nodestore: Error creating Cassandra table: " << rc << ", "
               << cass_error_desc(rc) << " - " << query.str();
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        query = {};
        query << "SELECT * FROM " << tableName << "flattransactions"
              << " LIMIT 1";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK)
        {
            if (rc == CASS_ERROR_SERVER_INVALID_QUERY)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << "table not here yet, sleeping 1s to "
                       "see if table creation propagates";
                continue;
            }
            else
            {
                std::stringstream ss;
                ss << "nodestore: Error checking for table: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << ss.str();
                continue;
            }
        }
        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tableName << "keys"
              << " ( key blob, created bigint, deleted bigint, PRIMARY KEY "
                 "(key, created)) with clustering order by (created "
                 "desc) ";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK && rc != CASS_ERROR_SERVER_INVALID_QUERY)
        {
            std::stringstream ss;
            ss << "nodestore: Error creating Cassandra table: " << rc << ", "
               << cass_error_desc(rc) << " - " << query.str();
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        query = {};
        query << "SELECT * FROM " << tableName << "keys"
              << " LIMIT 1";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK)
        {
            if (rc == CASS_ERROR_SERVER_INVALID_QUERY)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << "table not here yet, sleeping 1s to "
                       "see if table creation propagates";
                continue;
            }
            else
            {
                std::stringstream ss;
                ss << "nodestore: Error checking for table: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << ss.str();
                continue;
            }
        }
        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tableName << "books"
              << " ( book blob, sequence bigint, key blob, deleted_at "
                 "bigint, PRIMARY KEY "
                 "(book, key)) WITH CLUSTERING ORDER BY (key ASC)";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK && rc != CASS_ERROR_SERVER_INVALID_QUERY)
        {
            std::stringstream ss;
            ss << "nodestore: Error creating Cassandra table: " << rc << ", "
               << cass_error_desc(rc) << " - " << query.str();
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        query = {};
        query << "SELECT * FROM " << tableName << "books"
              << " LIMIT 1";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK)
        {
            if (rc == CASS_ERROR_SERVER_INVALID_QUERY)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << "table not here yet, sleeping 1s to "
                       "see if table creation propagates";
                continue;
            }
            else
            {
                std::stringstream ss;
                ss << "nodestore: Error checking for table: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << ss.str();
                continue;
            }
        }

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tableName << "account_tx"
              << " ( account blob, seq_idx tuple<bigint, bigint>, "
                 " hash blob, "
                 "PRIMARY KEY "
                 "(account, seq_idx)) WITH "
                 "CLUSTERING ORDER BY (seq_idx desc)";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK && rc != CASS_ERROR_SERVER_INVALID_QUERY)
        {
            std::stringstream ss;
            ss << "nodestore: Error creating Cassandra table: " << rc << ", "
               << cass_error_desc(rc) << " - " << query.str();
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        query = {};
        query << "SELECT * FROM " << tableName << "account_tx"
              << " LIMIT 1";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK)
        {
            if (rc == CASS_ERROR_SERVER_INVALID_QUERY)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << "table not here yet, sleeping 1s to "
                       "see if table creation propagates";
                continue;
            }
            else
            {
                std::stringstream ss;
                ss << "nodestore: Error checking for table: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << ss.str();
                continue;
            }
        }

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tableName << "ledgers"
              << " ( sequence bigint PRIMARY KEY, header blob )";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK && rc != CASS_ERROR_SERVER_INVALID_QUERY)
        {
            std::stringstream ss;
            ss << "nodestore: Error creating Cassandra table: " << rc << ", "
               << cass_error_desc(rc) << " - " << query.str();
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        query = {};
        query << "SELECT * FROM " << tableName << "ledgers"
              << " LIMIT 1";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK)
        {
            if (rc == CASS_ERROR_SERVER_INVALID_QUERY)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << "table not here yet, sleeping 1s to "
                       "see if table creation propagates";
                continue;
            }
            else
            {
                std::stringstream ss;
                ss << "nodestore: Error checking for table: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << ss.str();
                continue;
            }
        }

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tableName << "ledger_hashes"
              << " (hash blob PRIMARY KEY, sequence bigint)";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK && rc != CASS_ERROR_SERVER_INVALID_QUERY)
        {
            std::stringstream ss;
            ss << "nodestore: Error creating Cassandra table: " << rc << ", "
               << cass_error_desc(rc) << " - " << query.str();
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        query = {};
        query << "SELECT * FROM " << tableName << "ledger_hashes"
              << " LIMIT 1";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK)
        {
            if (rc == CASS_ERROR_SERVER_INVALID_QUERY)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << "table not here yet, sleeping 1s to "
                       "see if table creation propagates";
                continue;
            }
            else
            {
                std::stringstream ss;
                ss << "nodestore: Error checking for table: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << ss.str();
                continue;
            }
        }

        query = {};
        query << "CREATE TABLE IF NOT EXISTS " << tableName << "ledger_range"
              << " (is_latest boolean PRIMARY KEY, sequence bigint)";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK && rc != CASS_ERROR_SERVER_INVALID_QUERY)
        {
            std::stringstream ss;
            ss << "nodestore: Error creating Cassandra table: " << rc << ", "
               << cass_error_desc(rc) << " - " << query.str();
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        query = {};
        query << "SELECT * FROM " << tableName << "ledger_range"
              << " LIMIT 1";
        statement = makeStatement(query.str().c_str(), 0);
        fut = cass_session_execute(session_.get(), statement);
        rc = cass_future_error_code(fut);
        cass_future_free(fut);
        cass_statement_free(statement);
        if (rc != CASS_OK)
        {
            if (rc == CASS_ERROR_SERVER_INVALID_QUERY)
            {
                BOOST_LOG_TRIVIAL(warning)
                    << "table not here yet, sleeping 1s to "
                       "see if table creation propagates";
                continue;
            }
            else
            {
                std::stringstream ss;
                ss << "nodestore: Error checking for table: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << ss.str();
                continue;
            }
        }
        setupSessionAndTable = true;
    }

    cass_cluster_free(cluster);

    bool setupPreparedStatements = false;
    while (!setupPreparedStatements)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::stringstream query;
        query << "INSERT INTO " << tableName << "flat"
              << " (key, sequence, object) VALUES (?, ?, ?)";
        CassFuture* prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        /* Wait for the statement to prepare and get the result */
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            /* Handle error */
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing insert : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        /* Get the prepared object from the future */
        insertObject_ = cass_future_get_prepared(prepare_future);

        /* The future can be freed immediately after getting the prepared
         * object
         */
        cass_future_free(prepare_future);

        query = {};
        query << "INSERT INTO " << tableName << "flattransactions"
              << " (hash, sequence, transaction, metadata) VALUES (?, ?, "
                 "?, ?)";
        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        /* Wait for the statement to prepare and get the result */
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            /* Handle error */
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing insert : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        /* Get the prepared object from the future */
        insertTransaction_ = cass_future_get_prepared(prepare_future);
        cass_future_free(prepare_future);

        query = {};
        query << "INSERT INTO " << tableName << "keys"
              << " (key, created, deleted) VALUES (?, ?, ?)";
        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        /* Wait for the statement to prepare and get the result */
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            /* Handle error */
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing insert : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        /* Get the prepared object from the future */
        insertKey_ = cass_future_get_prepared(prepare_future);
        cass_future_free(prepare_future);

        query = {};
        query << "INSERT INTO " << tableName << "books"
              << " (book, key, sequence, deleted_at) VALUES (?, ?, ?, ?)";
        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        /* Wait for the statement to prepare and get the result */
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            /* Handle error */
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing insert : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        /* Get the prepared object from the future */
        insertBook_ = cass_future_get_prepared(prepare_future);
        cass_future_free(prepare_future);
        query = {};

        query << "INSERT INTO " << tableName << "books"
              << " (book, key, deleted_at) VALUES (?, ?, ?)";
        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        /* Wait for the statement to prepare and get the result */
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            /* Handle error */
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing insert : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        /* Get the prepared object from the future */
        deleteBook_ = cass_future_get_prepared(prepare_future);
        cass_future_free(prepare_future);

        query = {};
        query << "SELECT created FROM " << tableName << "keys"
              << " WHERE key = ? ORDER BY created desc LIMIT 1";
        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        /* Wait for the statement to prepare and get the result */
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            /* Handle error */
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing insert : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        /* Get the prepared object from the future */
        getCreated_ = cass_future_get_prepared(prepare_future);
        cass_future_free(prepare_future);

        query = {};
        query << "SELECT object, sequence FROM " << tableName << "flat"
              << " WHERE key = ? AND sequence <= ? ORDER BY sequence DESC "
                 "LIMIT 1";
        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        /* Wait for the statement to prepare and get the result */
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            /* Handle error */
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing select : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        /* Get the prepared object from the future */
        selectObject_ = cass_future_get_prepared(prepare_future);

        /* The future can be freed immediately after getting the prepared
         * object
         */
        cass_future_free(prepare_future);

        query = {};
        query << "SELECT transaction,metadata FROM " << tableName
              << "flattransactions"
              << " WHERE hash = ?";
        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        /* Wait for the statement to prepare and get the result */
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            /* Handle error */
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing select : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        /* Get the prepared object from the future */
        selectTransaction_ = cass_future_get_prepared(prepare_future);

        /* The future can be freed immediately after getting the prepared
         * object
         */
        cass_future_free(prepare_future);

        query = {};
        query << "SELECT key FROM " << tableName << "keys "
              << " WHERE TOKEN(key) >= ? and created <= ?"
              << " and deleted > ?"
              << " PER PARTITION LIMIT 1 LIMIT ?"
              << " ALLOW FILTERING";

        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        // Wait for the statement to prepare and get the result
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            // Handle error
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing upperBound : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str() << " : " << query.str();
            continue;
        }

        // Get the prepared object from the future
        upperBound_ = cass_future_get_prepared(prepare_future);

        // The future can be freed immediately after getting the prepared
        // object
        //
        cass_future_free(prepare_future);

        query = {};
        query << "SELECT filterempty(key,object) FROM " << tableName << "flat "
              << " WHERE TOKEN(key) >= ? and sequence <= ?"
              << " PER PARTITION LIMIT 1 LIMIT ?"
              << " ALLOW FILTERING";

        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        // Wait for the statement to prepare and get the result
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            // Handle error
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing upperBound : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str() << " : " << query.str();
            continue;
        }

        // Get the prepared object from the future
        upperBound2_ = cass_future_get_prepared(prepare_future);

        // The future can be freed immediately after getting the prepared
        // object
        //
        cass_future_free(prepare_future);
        query = {};
        query << "SELECT TOKEN(key) FROM " << tableName << "flat "
              << " WHERE key = ? LIMIT 1";

        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        // Wait for the statement to prepare and get the result
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            // Handle error
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing getToken : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        getToken_ = cass_future_get_prepared(prepare_future);

        query = {};
        query << "SELECT key FROM " << tableName << "books "
              << " WHERE book = ? AND sequence <= ? AND deleted_at > ? AND"
                 " key > ? "
                 " ORDER BY key ASC LIMIT ? ALLOW FILTERING";

        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        // Wait for the statement to prepare and get the result
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            // Handle error
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing getToken : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        getBook_ = cass_future_get_prepared(prepare_future);

        query = {};
        query << " INSERT INTO " << tableName << "account_tx"
              << " (account, seq_idx, hash) "
              << " VALUES (?,?,?)";

        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        // Wait for the statement to prepare and get the result
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            // Handle error
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing getToken : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        insertAccountTx_ = cass_future_get_prepared(prepare_future);
        query = {};
        query << " SELECT hash,seq_idx FROM " << tableName << "account_tx"
              << " WHERE account = ? "
              << " AND seq_idx < ? LIMIT ?";

        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        // Wait for the statement to prepare and get the result
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            // Handle error
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing getToken : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        selectAccountTx_ = cass_future_get_prepared(prepare_future);
        query = {};
        query << " INSERT INTO " << tableName << "ledgers "
              << " (sequence, header) VALUES(?,?)";

        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        // Wait for the statement to prepare and get the result
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            // Handle error
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing getToken : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        insertLedgerHeader_ = cass_future_get_prepared(prepare_future);
        query = {};
        query << " INSERT INTO " << tableName << "ledger_hashes"
              << " (hash, sequence) VALUES(?,?)";

        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        // Wait for the statement to prepare and get the result
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            // Handle error
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: Error preparing getToken : " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        insertLedgerHash_ = cass_future_get_prepared(prepare_future);
        query = {};
        query << " update " << tableName << "ledger_range"
              << " set sequence = ? where is_latest = ? if sequence != ?";

        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        // wait for the statement to prepare and get the result
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            // handle error
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: error preparing updateLedgerRange : " << rc
               << ", " << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        updateLedgerRange_ = cass_future_get_prepared(prepare_future);
        query = {};
        query << " select header from " << tableName
              << "ledgers where sequence = ?";

        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        // wait for the statement to prepare and get the result
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            // handle error
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: error preparing selectLedgerBySeq : " << rc
               << ", " << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        selectLedgerBySeq_ = cass_future_get_prepared(prepare_future);
        query = {};
        query << " select sequence from " << tableName
              << "ledger_range where is_latest = true";

        prepare_future =
            cass_session_prepare(session_.get(), query.str().c_str());

        // wait for the statement to prepare and get the result
        rc = cass_future_error_code(prepare_future);

        if (rc != CASS_OK)
        {
            // handle error
            cass_future_free(prepare_future);

            std::stringstream ss;
            ss << "nodestore: error preparing selectLatestLedger : " << rc
               << ", " << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << ss.str();
            continue;
        }

        selectLatestLedger_ = cass_future_get_prepared(prepare_future);

        setupPreparedStatements = true;
    }

    work_.emplace(ioContext_);
    ioThread_ = std::thread{[this]() { ioContext_.run(); }};
    open_ = true;

    if (config_.contains("max_requests_outstanding"))
    {
        maxRequestsOutstanding = config_["max_requests_outstanding"].as_int64();
    }
    BOOST_LOG_TRIVIAL(info) << "Opened database successfully";
}
}  // namespace Backend
