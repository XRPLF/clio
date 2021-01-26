//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2020 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#ifndef RIPPLE_APP_REPORTING_REPORTINGBACKEND_H_INCLUDED
#define RIPPLE_APP_REPORTING_REPORTINGBACKEND_H_INCLUDED

#include <ripple/basics/base_uint.h>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <atomic>
#include <cassandra.h>
#include <cstddef>
#include <iostream>
#include <memory>
#include <mutex>

void
flatMapWriteCallback(CassFuture* fut, void* cbData);
void
flatMapWriteKeyCallback(CassFuture* fut, void* cbData);
void
flatMapWriteTransactionCallback(CassFuture* fut, void* cbData);
void
flatMapWriteBookCallback(CassFuture* fut, void* cbData);
void
flatMapReadCallback(CassFuture* fut, void* cbData);
void
flatMapReadObjectCallback(CassFuture* fut, void* cbData);
void
flatMapGetCreatedCallback(CassFuture* fut, void* cbData);
class CassandraFlatMapBackend
{
private:
    // convenience function for one-off queries. For normal reads and writes,
    // use the prepared statements insert_ and select_
    CassStatement*
    makeStatement(char const* query, std::size_t params)
    {
        CassStatement* ret = cass_statement_new(query, params);
        CassError rc =
            cass_statement_set_consistency(ret, CASS_CONSISTENCY_QUORUM);
        if (rc != CASS_OK)
        {
            std::stringstream ss;
            ss << "nodestore: Error setting query consistency: " << query
               << ", result: " << rc << ", " << cass_error_desc(rc);
            throw std::runtime_error(ss.str());
        }
        return ret;
    }

    std::atomic<bool> open_{false};

    // mutex used for open() and close()
    std::mutex mutex_;

    std::unique_ptr<CassSession, void (*)(CassSession*)> session_{
        nullptr,
        [](CassSession* session) {
            // Try to disconnect gracefully.
            CassFuture* fut = cass_session_close(session);
            cass_future_wait(fut);
            cass_future_free(fut);
            cass_session_free(session);
        }};

    // Database statements cached server side. Using these is more efficient
    // than making a new statement
    const CassPrepared* insertObject_ = nullptr;
    const CassPrepared* insertTransaction_ = nullptr;
    const CassPrepared* selectTransaction_ = nullptr;
    const CassPrepared* selectObject_ = nullptr;
    const CassPrepared* upperBound_ = nullptr;
    const CassPrepared* getToken_ = nullptr;
    const CassPrepared* insertKey_ = nullptr;
    const CassPrepared* getCreated_ = nullptr;
    const CassPrepared* getBook_ = nullptr;
    const CassPrepared* insertBook_ = nullptr;

    // io_context used for exponential backoff for write retries
    mutable boost::asio::io_context ioContext_;
    std::optional<boost::asio::io_context::work> work_;
    std::thread ioThread_;

    // maximum number of concurrent in flight requests. New requests will wait
    // for earlier requests to finish if this limit is exceeded
    uint32_t maxRequestsOutstanding = 10000000;
    mutable std::atomic_uint32_t numRequestsOutstanding_ = 0;

    // mutex and condition_variable to limit the number of concurrent in flight
    // requests
    mutable std::mutex throttleMutex_;
    mutable std::condition_variable throttleCv_;

    // writes are asynchronous. This mutex and condition_variable is used to
    // wait for all writes to finish
    mutable std::mutex syncMutex_;
    mutable std::condition_variable syncCv_;

    boost::json::object config_;

public:
    CassandraFlatMapBackend(boost::json::object const& config) : config_(config)
    {
    }

    ~CassandraFlatMapBackend()
    {
        if (open_)
            close();
    }

    std::string
    getName()
    {
        return "cassandra";
    }

    bool
    isOpen()
    {
        return open_;
    }

    // Setup all of the necessary components for talking to the database.
    // Create the table if it doesn't exist already
    // @param createIfMissing ignored
    void
    open()
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
            throw std::runtime_error(
                "nodestore:: Failed to create CassCluster");

        std::string secureConnectBundle = getString("secure_connect_bundle");

        if (!secureConnectBundle.empty())
        {
            /* Setup driver to connect to the cloud using the secure connection
             * bundle */
            if (cass_cluster_set_cloud_secure_connection_bundle(
                    cluster, secureConnectBundle.c_str()) != CASS_OK)
            {
                BOOST_LOG_TRIVIAL(error)
                    << "Unable to configure cloud using the "
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
            CassError rc = cass_cluster_set_contact_points(
                cluster, contact_points.c_str());
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "nodestore: Error setting Cassandra contact_points: "
                   << contact_points << ", result: " << rc << ", "
                   << cass_error_desc(rc);

                throw std::runtime_error(ss.str());
            }

            int port =
                config_.contains("port") ? config_["port"].as_int64() : 0;
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
        CassError rc = cass_cluster_set_protocol_version(
            cluster, CASS_PROTOCOL_VERSION_V4);
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
                throw std::system_error(
                    errno, std::generic_category(), ss.str());
            }
            std::string cert(
                std::istreambuf_iterator<char>{fileStream},
                std::istreambuf_iterator<char>{});
            if (fileStream.bad())
            {
                std::stringstream ss;
                ss << "reading config file " << certfile;
                throw std::system_error(
                    errno, std::generic_category(), ss.str());
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
                ss << "nodestore: Error creating Cassandra table: " << rc
                   << ", " << cass_error_desc(rc) << " - " << query.str();
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
                ss << "nodestore: Error creating Cassandra table: " << rc
                   << ", " << cass_error_desc(rc) << " - " << query.str();
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
                ss << "nodestore: Error creating Cassandra table: " << rc
                   << ", " << cass_error_desc(rc) << " - " << query.str();
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
                     "bigint static, PRIMARY KEY "
                     "(book, sequence, key))";
            statement = makeStatement(query.str().c_str(), 0);
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            cass_future_free(fut);
            cass_statement_free(statement);
            if (rc != CASS_OK && rc != CASS_ERROR_SERVER_INVALID_QUERY)
            {
                std::stringstream ss;
                ss << "nodestore: Error creating Cassandra table: " << rc
                   << ", " << cass_error_desc(rc) << " - " << query.str();
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
                  << " (book, sequence, key, deleted_at) VALUES (?, ?, ?, ?)";
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
                  << " WHERE book = ? AND sequence <= ? AND deleted_at > ? "
                     "ALLOW FILTERING";

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

            setupPreparedStatements = true;
        }

        work_.emplace(ioContext_);
        ioThread_ = std::thread{[this]() { ioContext_.run(); }};
        open_ = true;

        if (config_.contains("max_requests_outstanding"))
        {
            maxRequestsOutstanding =
                config_["max_requests_outstanding"].as_int64();
        }
        BOOST_LOG_TRIVIAL(info) << "Opened database successfully";
    }

    // Close the connection to the database
    void
    close()
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (insertTransaction_)
            {
                cass_prepared_free(insertTransaction_);
                insertTransaction_ = nullptr;
            }
            if (insertObject_)
            {
                cass_prepared_free(insertObject_);
                insertObject_ = nullptr;
            }
            if (insertKey_)
            {
                cass_prepared_free(insertKey_);
                insertKey_ = nullptr;
            }
            if (selectTransaction_)
            {
                cass_prepared_free(selectTransaction_);
                selectTransaction_ = nullptr;
            }
            if (selectObject_)
            {
                cass_prepared_free(selectObject_);
                selectObject_ = nullptr;
            }
            if (upperBound_)
            {
                cass_prepared_free(upperBound_);
                upperBound_ = nullptr;
            }
            if (getToken_)
            {
                cass_prepared_free(getToken_);
                getToken_ = nullptr;
            }
            if (getCreated_)
            {
                cass_prepared_free(getCreated_);
                getCreated_ = nullptr;
            }
            if (getBook_)
            {
                cass_prepared_free(getBook_);
                getBook_ = nullptr;
            }
            work_.reset();
            ioThread_.join();
        }
        open_ = false;
    }

    // Synchronously fetch the object with key key and store the result in pno
    // @param key the key of the object
    // @param pno object in which to store the result
    // @return result status of query
    std::optional<std::vector<unsigned char>>
    fetch(void const* key, uint32_t sequence) const
    {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        auto start = std::chrono::system_clock::now();
        CassStatement* statement = cass_prepared_bind(selectObject_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement, 0, static_cast<cass_byte_t const*>(key), 32);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        rc = cass_statement_bind_int64(statement, 1, sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch error: no rows";
            cass_result_free(res);
            return {};
        }
        cass_byte_t const* buf;
        std::size_t bufSize;
        rc = cass_value_get_bytes(cass_row_get_column(row, 0), &buf, &bufSize);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch result error: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        std::vector<unsigned char> result{buf, buf + bufSize};
        cass_result_free(res);
        auto end = std::chrono::system_clock::now();
        BOOST_LOG_TRIVIAL(debug)
            << "Fetched from cassandra in "
            << std::chrono::duration_cast<std::chrono::microseconds>(
                   end - start)
                   .count()
            << " microseconds";
        return result;
    }

    std::optional<int64_t>
    getToken(void const* key) const
    {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        auto start = std::chrono::system_clock::now();
        CassStatement* statement = cass_prepared_bind(getToken_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement, 0, static_cast<cass_byte_t const*>(key), 32);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch error: no rows";
            cass_result_free(res);
            return {};
        }
        cass_int64_t token;
        rc = cass_value_get_int64(cass_row_get_column(row, 0), &token);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch result error: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        cass_result_free(res);
        if (token == INT64_MAX)
            return {};
        return token + 1;
    }

    std::optional<
        std::pair<std::vector<unsigned char>, std::vector<unsigned char>>>
    fetchTransaction(void const* hash) const
    {
        BOOST_LOG_TRIVIAL(trace) << "Fetching from cassandra";
        auto start = std::chrono::system_clock::now();
        CassStatement* statement = cass_prepared_bind(selectTransaction_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement, 0, static_cast<cass_byte_t const*>(hash), 32);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        CassRow const* row = cass_result_first_row(res);
        if (!row)
        {
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch error: no rows";
            cass_result_free(res);
            return {};
        }
        cass_byte_t const* txBuf;
        std::size_t txBufSize;
        rc = cass_value_get_bytes(
            cass_row_get_column(row, 0), &txBuf, &txBufSize);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch result error: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        std::vector<unsigned char> txResult{txBuf, txBuf + txBufSize};
        cass_byte_t const* metaBuf;
        std::size_t metaBufSize;
        rc = cass_value_get_bytes(
            cass_row_get_column(row, 0), &metaBuf, &metaBufSize);
        if (rc != CASS_OK)
        {
            cass_result_free(res);
            BOOST_LOG_TRIVIAL(error) << "Cassandra fetch result error: " << rc
                                     << ", " << cass_error_desc(rc);
            return {};
        }
        std::vector<unsigned char> metaResult{metaBuf, metaBuf + metaBufSize};
        cass_result_free(res);
        auto end = std::chrono::system_clock::now();
        BOOST_LOG_TRIVIAL(debug)
            << "Fetched from cassandra in "
            << std::chrono::duration_cast<std::chrono::microseconds>(
                   end - start)
                   .count()
            << " microseconds";
        return {{txResult, metaResult}};
    }
    struct LedgerObject
    {
        ripple::uint256 key;
        std::vector<unsigned char> blob;
    };
    std::pair<std::vector<LedgerObject>, std::optional<int64_t>>
    doUpperBound(
        std::optional<int64_t> marker,
        std::uint32_t seq,
        std::uint32_t limit) const
    {
        BOOST_LOG_TRIVIAL(debug) << "Starting doUpperBound";
        CassStatement* statement = cass_prepared_bind(upperBound_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        int64_t markerVal = marker ? marker.value() : INT64_MIN;

        CassError rc = cass_statement_bind_int64(statement, 0, markerVal);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra hash to doUpperBound query: " << rc
                << ", " << cass_error_desc(rc);
            return {};
        }

        rc = cass_statement_bind_int64(statement, 1, seq);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra seq to doUpperBound query: " << rc << ", "
                << cass_error_desc(rc);
            return {};
        }
        rc = cass_statement_bind_int64(statement, 2, seq);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra seq to doUpperBound query: " << rc << ", "
                << cass_error_desc(rc);
            return {};
        }

        rc = cass_statement_bind_int32(statement, 3, limit);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra limit to doUpperBound query: " << rc
                << ", " << cass_error_desc(rc);
            return {};
        }

        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        BOOST_LOG_TRIVIAL(debug) << "doUpperBound - got keys";
        std::vector<ripple::uint256> keys;

        CassIterator* iter = cass_iterator_from_result(res);
        while (cass_iterator_next(iter))
        {
            CassRow const* row = cass_iterator_get_row(iter);

            cass_byte_t const* outData;
            std::size_t outSize;

            CassValue const* hash = cass_row_get_column(row, 0);
            rc = cass_value_get_bytes(hash, &outData, &outSize);
            if (rc != CASS_OK)
            {
                cass_iterator_free(iter);

                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
            keys.push_back(ripple::uint256::fromVoid(outData));
        }
        BOOST_LOG_TRIVIAL(debug)
            << "doUpperBound - populated keys. num keys = " << keys.size();
        if (keys.size())
        {
            std::vector<LedgerObject> results;
            std::vector<Blob> objs = fetchObjectsBatch(keys, seq);
            for (size_t i = 0; i < objs.size(); ++i)
            {
                results.push_back({keys[i], objs[i]});
            }
            auto token = getToken(results[results.size() - 1].key.data());
            assert(token);
            return std::make_pair(results, token);
        }

        return {{}, {}};
    }

    std::vector<LedgerObject>
    doBookOffers(std::vector<unsigned char> const& book, uint32_t sequence)
        const
    {
        BOOST_LOG_TRIVIAL(debug) << "Starting doBookOffers";
        CassStatement* statement = cass_prepared_bind(upperBound_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement, 0, static_cast<cass_byte_t const*>(book.data()), 32);

        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra book to doBookOffers query: " << rc
                << ", " << cass_error_desc(rc);
            return {};
        }

        rc = cass_statement_bind_int64(statement, 1, sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra sequence to doBookOffers query: " << rc
                << ", " << cass_error_desc(rc);
            return {};
        }
        rc = cass_statement_bind_int64(statement, 2, sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error)
                << "Binding Cassandra deleted_at to doBookOffers query: " << rc
                << ", " << cass_error_desc(rc);
            return {};
        }

        CassFuture* fut;
        do
        {
            fut = cass_session_execute(session_.get(), statement);
            rc = cass_future_error_code(fut);
            if (rc != CASS_OK)
            {
                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ", retrying";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
        } while (rc != CASS_OK);

        CassResult const* res = cass_future_get_result(fut);
        cass_statement_free(statement);
        cass_future_free(fut);

        BOOST_LOG_TRIVIAL(debug) << "doUpperBound - got keys";
        std::vector<ripple::uint256> keys;

        CassIterator* iter = cass_iterator_from_result(res);
        while (cass_iterator_next(iter))
        {
            CassRow const* row = cass_iterator_get_row(iter);

            cass_byte_t const* outData;
            std::size_t outSize;

            CassValue const* hash = cass_row_get_column(row, 0);
            rc = cass_value_get_bytes(hash, &outData, &outSize);
            if (rc != CASS_OK)
            {
                cass_iterator_free(iter);

                std::stringstream ss;
                ss << "Cassandra fetch error";
                ss << ": " << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(warning) << ss.str();
            }
            keys.push_back(ripple::uint256::fromVoid(outData));
        }
        BOOST_LOG_TRIVIAL(debug)
            << "doUpperBound - populated keys. num keys = " << keys.size();
        if (keys.size())
        {
            std::vector<LedgerObject> results;
            std::vector<Blob> objs = fetchObjectsBatch(keys, sequence);
            for (size_t i = 0; i < objs.size(); ++i)
            {
                results.push_back({keys[i], objs[i]});
            }
            return results;
        }

        return {};
    }
    bool
    canFetchBatch()
    {
        return true;
    }

    using Blob = std::vector<unsigned char>;
    using BlobPair = std::pair<Blob, Blob>;

    struct ReadCallbackData
    {
        CassandraFlatMapBackend const& backend;
        ripple::uint256 const& hash;
        BlobPair& result;
        std::condition_variable& cv;

        std::atomic_uint32_t& numFinished;
        size_t batchSize;

        ReadCallbackData(
            CassandraFlatMapBackend const& backend,
            ripple::uint256 const& hash,
            BlobPair& result,
            std::condition_variable& cv,
            std::atomic_uint32_t& numFinished,
            size_t batchSize)
            : backend(backend)
            , hash(hash)
            , result(result)
            , cv(cv)
            , numFinished(numFinished)
            , batchSize(batchSize)
        {
        }

        ReadCallbackData(ReadCallbackData const& other) = default;
    };

    std::vector<BlobPair>
    fetchBatch(std::vector<ripple::uint256> const& hashes) const
    {
        std::size_t const numHashes = hashes.size();
        BOOST_LOG_TRIVIAL(trace)
            << "Fetching " << numHashes << " records from Cassandra";
        std::atomic_uint32_t numFinished = 0;
        std::condition_variable cv;
        std::mutex mtx;
        std::vector<BlobPair> results{numHashes};
        std::vector<std::shared_ptr<ReadCallbackData>> cbs;
        cbs.reserve(numHashes);
        for (std::size_t i = 0; i < hashes.size(); ++i)
        {
            cbs.push_back(std::make_shared<ReadCallbackData>(
                *this, hashes[i], results[i], cv, numFinished, numHashes));
            read(*cbs[i]);
        }
        assert(results.size() == cbs.size());

        std::unique_lock<std::mutex> lck(mtx);
        cv.wait(lck, [&numFinished, &numHashes]() {
            return numFinished == numHashes;
        });

        BOOST_LOG_TRIVIAL(trace)
            << "Fetched " << numHashes << " records from Cassandra";
        return results;
    }

    void
    read(ReadCallbackData& data) const
    {
        CassStatement* statement = cass_prepared_bind(selectTransaction_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement,
            0,
            static_cast<cass_byte_t const*>(data.hash.data()),
            32);
        if (rc != CASS_OK)
        {
            size_t batchSize = data.batchSize;
            if (++(data.numFinished) == batchSize)
                data.cv.notify_all();
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return;
        }

        CassFuture* fut = cass_session_execute(session_.get(), statement);

        cass_statement_free(statement);

        cass_future_set_callback(
            fut, flatMapReadCallback, static_cast<void*>(&data));
        cass_future_free(fut);
    }

    struct ReadObjectCallbackData
    {
        CassandraFlatMapBackend const& backend;
        ripple::uint256 const& key;
        uint32_t sequence;
        Blob& result;
        std::condition_variable& cv;

        std::atomic_uint32_t& numFinished;
        size_t batchSize;

        ReadObjectCallbackData(
            CassandraFlatMapBackend const& backend,
            ripple::uint256 const& key,
            uint32_t sequence,
            Blob& result,
            std::condition_variable& cv,
            std::atomic_uint32_t& numFinished,
            size_t batchSize)
            : backend(backend)
            , key(key)
            , sequence(sequence)
            , result(result)
            , cv(cv)
            , numFinished(numFinished)
            , batchSize(batchSize)
        {
        }

        ReadObjectCallbackData(ReadObjectCallbackData const& other) = default;
    };
    std::vector<Blob>
    fetchObjectsBatch(
        std::vector<ripple::uint256> const& keys,
        uint32_t sequence) const
    {
        std::size_t const numKeys = keys.size();
        BOOST_LOG_TRIVIAL(trace)
            << "Fetching " << numKeys << " records from Cassandra";
        std::atomic_uint32_t numFinished = 0;
        std::condition_variable cv;
        std::mutex mtx;
        std::vector<Blob> results{numKeys};
        std::vector<std::shared_ptr<ReadObjectCallbackData>> cbs;
        cbs.reserve(numKeys);
        for (std::size_t i = 0; i < keys.size(); ++i)
        {
            cbs.push_back(std::make_shared<ReadObjectCallbackData>(
                *this,
                keys[i],
                sequence,
                results[i],
                cv,
                numFinished,
                numKeys));
            readObject(*cbs[i]);
        }
        assert(results.size() == cbs.size());

        std::unique_lock<std::mutex> lck(mtx);
        cv.wait(
            lck, [&numFinished, &numKeys]() { return numFinished == numKeys; });

        BOOST_LOG_TRIVIAL(trace)
            << "Fetched " << numKeys << " records from Cassandra";
        return results;
    }
    void
    readObject(ReadObjectCallbackData& data) const
    {
        CassStatement* statement = cass_prepared_bind(selectObject_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        CassError rc = cass_statement_bind_bytes(
            statement, 0, static_cast<cass_byte_t const*>(data.key.data()), 32);
        if (rc != CASS_OK)
        {
            size_t batchSize = data.batchSize;
            if (++(data.numFinished) == batchSize)
                data.cv.notify_all();
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return;
        }
        rc = cass_statement_bind_int64(statement, 1, data.sequence);

        if (rc != CASS_OK)
        {
            size_t batchSize = data.batchSize;
            if (++(data.numFinished) == batchSize)
                data.cv.notify_all();
            cass_statement_free(statement);
            BOOST_LOG_TRIVIAL(error) << "Binding Cassandra fetch query: " << rc
                                     << ", " << cass_error_desc(rc);
            return;
        }

        CassFuture* fut = cass_session_execute(session_.get(), statement);

        cass_statement_free(statement);

        cass_future_set_callback(
            fut, flatMapReadObjectCallback, static_cast<void*>(&data));
        cass_future_free(fut);
    }

    struct WriteCallbackData
    {
        CassandraFlatMapBackend const* backend;
        // The shared pointer to the node object must exist until it's
        // confirmed persisted. Otherwise, it can become deleted
        // prematurely if other copies are removed from caches.
        std::string key;
        uint32_t sequence;
        uint32_t createdSequence = 0;
        std::string blob;
        bool isCreated;
        bool isDeleted;
        std::optional<ripple::uint256> book;

        uint32_t currentRetries = 0;
        std::atomic<int> refs = 1;

        WriteCallbackData(
            CassandraFlatMapBackend const* f,
            std::string&& key,
            uint32_t sequence,
            std::string&& blob,
            bool isCreated,
            bool isDeleted,
            std::optional<ripple::uint256>&& book)
            : backend(f)
            , key(std::move(key))
            , sequence(sequence)
            , blob(std::move(blob))
            , isCreated(isCreated)
            , isDeleted(isDeleted)
            , book(std::move(book))
        {
            if (isCreated or isDeleted)
                ++refs;
            if (book)
                ++refs;
        }
    };

    void
    write(WriteCallbackData& data, bool isRetry) const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(trace)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }
        {
            CassStatement* statement = cass_prepared_bind(insertObject_);
            cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
            const unsigned char* keyData = (unsigned char*)data.key.data();
            CassError rc = cass_statement_bind_bytes(
                statement,
                0,
                static_cast<cass_byte_t const*>(keyData),
                data.key.size());
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding cassandra insert hash: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            rc = cass_statement_bind_int64(statement, 1, data.sequence);
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding cassandra insert object: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            const unsigned char* blobData = (unsigned char*)data.blob.data();
            rc = cass_statement_bind_bytes(
                statement,
                2,
                static_cast<cass_byte_t const*>(blobData),
                data.blob.size());
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding cassandra insert hash: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            CassFuture* fut = cass_session_execute(session_.get(), statement);
            cass_statement_free(statement);

            cass_future_set_callback(
                fut, flatMapWriteCallback, static_cast<void*>(&data));
            cass_future_free(fut);
        }
    }

    void
    writeDeletedKey(WriteCallbackData& data, bool isRetry) const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(trace)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }
        CassStatement* statement = cass_prepared_bind(insertKey_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        const unsigned char* keyData = (unsigned char*)data.key.data();
        CassError rc = cass_statement_bind_bytes(
            statement,
            0,
            static_cast<cass_byte_t const*>(keyData),
            data.key.size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert hash: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_statement_bind_int64(statement, 1, data.createdSequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "binding cassandra insert object: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_statement_bind_int64(statement, 2, data.sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "binding cassandra insert object: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        CassFuture* fut = cass_session_execute(session_.get(), statement);
        cass_statement_free(statement);

        cass_future_set_callback(
            fut, flatMapWriteKeyCallback, static_cast<void*>(&data));
        cass_future_free(fut);
    }

    void
    writeKey(WriteCallbackData& data, bool isRetry) const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(trace)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }
        if (data.isCreated)
        {
            CassStatement* statement = cass_prepared_bind(insertKey_);
            cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
            const unsigned char* keyData = (unsigned char*)data.key.data();
            CassError rc = cass_statement_bind_bytes(
                statement,
                0,
                static_cast<cass_byte_t const*>(keyData),
                data.key.size());
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding cassandra insert hash: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            rc = cass_statement_bind_int64(statement, 1, data.sequence);
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "binding cassandra insert object: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            rc = cass_statement_bind_int64(statement, 2, INT64_MAX);
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "binding cassandra insert object: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            CassFuture* fut = cass_session_execute(session_.get(), statement);
            cass_statement_free(statement);

            cass_future_set_callback(
                fut, flatMapWriteKeyCallback, static_cast<void*>(&data));
            cass_future_free(fut);
        }
        else if (data.isDeleted)
        {
            CassStatement* statement = cass_prepared_bind(getCreated_);
            cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
            const unsigned char* keyData = (unsigned char*)data.key.data();
            CassError rc = cass_statement_bind_bytes(
                statement,
                0,
                static_cast<cass_byte_t const*>(keyData),
                data.key.size());
            if (rc != CASS_OK)
            {
                cass_statement_free(statement);
                std::stringstream ss;
                ss << "Binding cassandra insert hash: " << rc << ", "
                   << cass_error_desc(rc);
                BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
                throw std::runtime_error(ss.str());
            }
            CassFuture* fut = cass_session_execute(session_.get(), statement);
            cass_statement_free(statement);

            cass_future_set_callback(
                fut, flatMapGetCreatedCallback, static_cast<void*>(&data));
            cass_future_free(fut);
        }
    }

    void
    writeBook(WriteCallbackData& data, bool isRetry) const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(trace)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }
        CassStatement* statement = cass_prepared_bind(insertBook_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        const unsigned char* bookData = (unsigned char*)data.book->data();
        CassError rc = cass_statement_bind_bytes(
            statement,
            0,
            static_cast<cass_byte_t const*>(bookData),
            data.book->size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert hash: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_statement_bind_int64(
            statement, 1, (data.isCreated ? data.sequence : 0));
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "binding cassandra insert object: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        const unsigned char* keyData = (unsigned char*)data.key.data();
        rc = cass_statement_bind_bytes(
            statement,
            2,
            static_cast<cass_byte_t const*>(keyData),
            data.key.size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert hash: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_statement_bind_int64(
            statement, 3, (data.isDeleted ? data.sequence : INT64_MAX));
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;

            ss << "binding cassandra insert object: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        CassFuture* fut = cass_session_execute(session_.get(), statement);
        cass_statement_free(statement);

        cass_future_set_callback(
            fut, flatMapWriteBookCallback, static_cast<void*>(&data));
        cass_future_free(fut);
    }
    void
    store(
        std::string&& key,
        uint32_t seq,
        std::string&& blob,
        bool isCreated,
        bool isDeleted,
        std::optional<ripple::uint256>&& book) const
    {
        BOOST_LOG_TRIVIAL(trace) << "Writing ledger object to cassandra";
        WriteCallbackData* data = new WriteCallbackData(
            this,
            std::move(key),
            seq,
            std::move(blob),
            isCreated,
            isDeleted,
            std::move(book));

        ++numRequestsOutstanding_;
        if (isCreated || isDeleted)
            ++numRequestsOutstanding_;
        write(*data, false);
        if (isCreated || isDeleted)
            writeKey(*data, false);
        if (book)
            writeBook(*data, false);
        // handle book
    }

    struct WriteTransactionCallbackData
    {
        CassandraFlatMapBackend const* backend;
        // The shared pointer to the node object must exist until it's
        // confirmed persisted. Otherwise, it can become deleted
        // prematurely if other copies are removed from caches.
        std::string hash;
        uint32_t sequence;
        std::string transaction;
        std::string metadata;

        uint32_t currentRetries = 0;

        WriteTransactionCallbackData(
            CassandraFlatMapBackend const* f,
            std::string&& hash,
            uint32_t sequence,
            std::string&& transaction,
            std::string&& metadata)
            : backend(f)
            , hash(std::move(hash))
            , sequence(sequence)
            , transaction(std::move(transaction))
            , metadata(std::move(metadata))
        {
        }
    };

    void
    writeTransaction(WriteTransactionCallbackData& data, bool isRetry) const
    {
        {
            std::unique_lock<std::mutex> lck(throttleMutex_);
            if (!isRetry && numRequestsOutstanding_ > maxRequestsOutstanding)
            {
                BOOST_LOG_TRIVIAL(trace)
                    << __func__ << " : "
                    << "Max outstanding requests reached. "
                    << "Waiting for other requests to finish";
                throttleCv_.wait(lck, [this]() {
                    return numRequestsOutstanding_ < maxRequestsOutstanding;
                });
            }
        }

        CassStatement* statement = cass_prepared_bind(insertTransaction_);
        cass_statement_set_consistency(statement, CASS_CONSISTENCY_QUORUM);
        const unsigned char* hashData = (unsigned char*)data.hash.data();
        CassError rc = cass_statement_bind_bytes(
            statement,
            0,
            static_cast<cass_byte_t const*>(hashData),
            data.hash.size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert hash: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        rc = cass_statement_bind_int64(statement, 1, data.sequence);
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert object: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        const unsigned char* transactionData =
            (unsigned char*)data.transaction.data();
        rc = cass_statement_bind_bytes(
            statement,
            2,
            static_cast<cass_byte_t const*>(transactionData),
            data.transaction.size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert hash: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        const unsigned char* metadata = (unsigned char*)data.metadata.data();
        rc = cass_statement_bind_bytes(
            statement,
            3,
            static_cast<cass_byte_t const*>(metadata),
            data.metadata.size());
        if (rc != CASS_OK)
        {
            cass_statement_free(statement);
            std::stringstream ss;
            ss << "Binding cassandra insert hash: " << rc << ", "
               << cass_error_desc(rc);
            BOOST_LOG_TRIVIAL(error) << __func__ << " : " << ss.str();
            throw std::runtime_error(ss.str());
        }
        CassFuture* fut = cass_session_execute(session_.get(), statement);
        cass_statement_free(statement);

        cass_future_set_callback(
            fut, flatMapWriteTransactionCallback, static_cast<void*>(&data));
        cass_future_free(fut);
    }
    void
    storeTransaction(
        std::string&& hash,
        uint32_t seq,
        std::string&& transaction,
        std::string&& metadata)
    {
        BOOST_LOG_TRIVIAL(trace) << "Writing txn to cassandra";
        WriteTransactionCallbackData* data = new WriteTransactionCallbackData(
            this,
            std::move(hash),
            seq,
            std::move(transaction),
            std::move(metadata));

        ++numRequestsOutstanding_;
        writeTransaction(*data, false);
    }

    void
    sync()
    {
        std::unique_lock<std::mutex> lck(syncMutex_);

        syncCv_.wait(lck, [this]() { return numRequestsOutstanding_ == 0; });
    }

    friend void
    flatMapWriteCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapWriteKeyCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapWriteTransactionCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapWriteBookCallback(CassFuture* fut, void* cbData);

    friend void
    flatMapReadCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapReadObjectCallback(CassFuture* fut, void* cbData);
    friend void
    flatMapGetCreatedCallback(CassFuture* fut, void* cbData);
};

#endif
