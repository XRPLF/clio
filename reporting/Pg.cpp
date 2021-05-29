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

// Need raw socket manipulation to determine if postgres socket IPv4 or 6.
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

#include <boost/asio/ssl/detail/openssl_init.hpp>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iterator>
#include <reporting/Pg.h>
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

static void
noticeReceiver(void* arg, PGresult const* res)
{
    BOOST_LOG_TRIVIAL(trace) << "server message: " << PQresultErrorMessage(res);
}

//-----------------------------------------------------------------------------

std::string
PgResult::msg() const
{
    if (error_.has_value())
    {
        std::stringstream ss;
        ss << error_->first << ": " << error_->second;
        return ss.str();
    }
    if (result_)
        return "ok";

    // Must be stopping.
    return "stopping";
}

//-----------------------------------------------------------------------------

/*
 Connecting described in:
 https://www.postgresql.org/docs/10/libpq-connect.html
 */
void
Pg::connect()
{
    if (conn_)
    {
        // Nothing to do if we already have a good connection.
        if (PQstatus(conn_.get()) == CONNECTION_OK)
            return;
        /* Try resetting connection. */
        PQreset(conn_.get());
    }
    else  // Make new connection.
    {
        conn_.reset(PQconnectdbParams(
            reinterpret_cast<char const* const*>(&config_.keywordsIdx[0]),
            reinterpret_cast<char const* const*>(&config_.valuesIdx[0]),
            0));
        if (!conn_)
            throw std::runtime_error("No db connection struct");
    }

    /** Results from a synchronous connection attempt can only be either
     * CONNECTION_OK or CONNECTION_BAD. */
    if (PQstatus(conn_.get()) == CONNECTION_BAD)
    {
        std::stringstream ss;
        ss << "DB connection status " << PQstatus(conn_.get()) << ": "
           << PQerrorMessage(conn_.get());
        throw std::runtime_error(ss.str());
    }

    // Log server session console messages.
    PQsetNoticeReceiver(conn_.get(), noticeReceiver, nullptr);
}

PgResult
Pg::query(char const* command, std::size_t nParams, char const* const* values)
{
    // The result object must be freed using the libpq API PQclear() call.
    pg_result_type ret{nullptr, [](PGresult* result) { PQclear(result); }};
    // Connect then submit query.
    while (true)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_)
                return PgResult();
        }
        try
        {
            connect();
            if (nParams)
            {
                // PQexecParams can process only a single command.
                ret.reset(PQexecParams(
                    conn_.get(),
                    command,
                    nParams,
                    nullptr,
                    values,
                    nullptr,
                    nullptr,
                    0));
            }
            else
            {
                // PQexec can process multiple commands separated by
                // semi-colons. Returns the response from the last
                // command processed.
                ret.reset(PQexec(conn_.get(), command));
            }
            if (!ret)
                throw std::runtime_error("no result structure returned");
            break;
        }
        catch (std::exception const& e)
        {
            // Sever connection and retry until successful.
            disconnect();
            BOOST_LOG_TRIVIAL(error)
                << "database error, retrying: " << e.what();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    // Ensure proper query execution.
    switch (PQresultStatus(ret.get()))
    {
        case PGRES_TUPLES_OK:
        case PGRES_COMMAND_OK:
        case PGRES_COPY_IN:
        case PGRES_COPY_OUT:
        case PGRES_COPY_BOTH:
            break;
        default: {
            std::stringstream ss;
            ss << "bad query result: " << PQresStatus(PQresultStatus(ret.get()))
               << " error message: " << PQerrorMessage(conn_.get())
               << ", number of tuples: " << PQntuples(ret.get())
               << ", number of fields: " << PQnfields(ret.get());
            BOOST_LOG_TRIVIAL(error) << ss.str();
            PgResult retRes(ret.get(), conn_.get());
            disconnect();

            return retRes;
        }
    }

    return PgResult(std::move(ret));
}

static pg_formatted_params
formatParams(pg_params const& dbParams)
{
    std::vector<std::optional<std::string>> const& values = dbParams.second;
    /* Convert vector to C-style array of C-strings for postgres API.
       std::nullopt is a proxy for NULL since an empty std::string is
       0 length but not NULL. */
    std::vector<char const*> valuesIdx;
    valuesIdx.reserve(values.size());
    std::stringstream ss;
    bool first = true;
    for (auto const& value : values)
    {
        if (value)
        {
            valuesIdx.push_back(value->c_str());
            ss << value->c_str();
        }
        else
        {
            valuesIdx.push_back(nullptr);
            ss << "(null)";
        }
        if (first)
            first = false;
        else
            ss << ',';
    }

    BOOST_LOG_TRIVIAL(trace)
        << "query: " << dbParams.first << ". params: " << ss.str();
    return valuesIdx;
}

PgResult
Pg::query(pg_params const& dbParams)
{
    char const* const& command = dbParams.first;
    auto const formattedParams = formatParams(dbParams);
    return query(
        command,
        formattedParams.size(),
        formattedParams.size()
            ? reinterpret_cast<char const* const*>(&formattedParams[0])
            : nullptr);
}

void
Pg::bulkInsert(char const* table, std::string const& records)
{
    // https://www.postgresql.org/docs/12/libpq-copy.html#LIBPQ-COPY-SEND
    assert(conn_.get());
    auto copyCmd = boost::format(R"(COPY %s FROM stdin)");
    auto formattedCmd = boost::str(copyCmd % table);
    BOOST_LOG_TRIVIAL(debug) << __func__ << " " << formattedCmd;
    auto res = query(formattedCmd.c_str());
    if (!res || res.status() != PGRES_COPY_IN)
    {
        std::stringstream ss;
        ss << "bulkInsert to " << table
           << ". Postgres insert error: " << res.msg();
        if (res)
            ss << ". Query status not PGRES_COPY_IN: " << res.status();
        BOOST_LOG_TRIVIAL(error) << __func__ << " " << records;
        throw std::runtime_error(ss.str());
    }

    if (PQputCopyData(conn_.get(), records.c_str(), records.size()) == -1)
    {
        std::stringstream ss;
        ss << "bulkInsert to " << table
           << ". PQputCopyData error: " << PQerrorMessage(conn_.get());
        disconnect();
        BOOST_LOG_TRIVIAL(error) << __func__ << " " << records;
        throw std::runtime_error(ss.str());
    }

    if (PQputCopyEnd(conn_.get(), nullptr) == -1)
    {
        std::stringstream ss;
        ss << "bulkInsert to " << table
           << ". PQputCopyEnd error: " << PQerrorMessage(conn_.get());
        disconnect();
        BOOST_LOG_TRIVIAL(error) << __func__ << " " << records;
        throw std::runtime_error(ss.str());
    }

    // The result object must be freed using the libpq API PQclear() call.
    pg_result_type copyEndResult{
        nullptr, [](PGresult* result) { PQclear(result); }};
    copyEndResult.reset(PQgetResult(conn_.get()));
    ExecStatusType status = PQresultStatus(copyEndResult.get());
    if (status != PGRES_COMMAND_OK)
    {
        std::stringstream ss;
        ss << "bulkInsert to " << table
           << ". PQputCopyEnd status not PGRES_COMMAND_OK: " << status
           << " message = " << PQerrorMessage(conn_.get());
        disconnect();
        BOOST_LOG_TRIVIAL(error) << __func__ << " " << records;
        throw std::runtime_error(ss.str());
    }
}

bool
Pg::clear()
{
    if (!conn_)
        return false;

    // The result object must be freed using the libpq API PQclear() call.
    pg_result_type res{nullptr, [](PGresult* result) { PQclear(result); }};

    // Consume results until no more, or until the connection is severed.
    do
    {
        res.reset(PQgetResult(conn_.get()));
        if (!res)
            break;

        // Pending bulk copy operations may leave the connection in such a
        // state that it must be disconnected.
        switch (PQresultStatus(res.get()))
        {
            case PGRES_COPY_IN:
                if (PQputCopyEnd(conn_.get(), nullptr) != -1)
                    break;
                [[fallthrough]];  // avoids compiler warning
            case PGRES_COPY_OUT:
            case PGRES_COPY_BOTH:
                conn_.reset();
            default:;
        }
    } while (res && conn_);

    return conn_ != nullptr;
}

//-----------------------------------------------------------------------------

PgPool::PgPool(boost::json::object const& config)
{
    // Make sure that boost::asio initializes the SSL library.
    {
        static boost::asio::ssl::detail::openssl_init<true> initSsl;
    }
    // Don't have postgres client initialize SSL.
    PQinitOpenSSL(0, 0);

    /*
    Connect to postgres to create low level connection parameters
    with optional caching of network address info for subsequent connections.
    See https://www.postgresql.org/docs/10/libpq-connect.html

    For bounds checking of postgres connection data received from
    the network: the largest size for any connection field in
    PG source code is 64 bytes as of 5/2019. There are 29 fields.
    */
    constexpr std::size_t maxFieldSize = 1024;
    constexpr std::size_t maxFields = 1000;
    std::string conninfo = "postgres://";
    auto getFieldAsString = [&config](auto field) {
        if (!config.contains(field))
            throw std::runtime_error(
                field + std::string{" missing from postgres config"});
        if (!config.at(field).is_string())
            throw std::runtime_error(
                field + std::string{" in postgres config is not a string"});
        return std::string{config.at(field).as_string().c_str()};
    };
    conninfo += getFieldAsString("username");
    conninfo += ":";
    conninfo += getFieldAsString("password");
    conninfo += "@";
    conninfo += getFieldAsString("contact_point");
    conninfo += "/";
    conninfo += getFieldAsString("database");

    // The connection object must be freed using the libpq API PQfinish() call.
    pg_connection_type conn(
        PQconnectdb(conninfo.c_str()), [](PGconn* conn) { PQfinish(conn); });
    if (!conn)
        throw std::runtime_error("Can't create DB connection.");
    if (PQstatus(conn.get()) != CONNECTION_OK)
    {
        std::stringstream ss;
        ss << "Initial DB connection failed: " << PQerrorMessage(conn.get());
        throw std::runtime_error(ss.str());
    }

    int const sockfd = PQsocket(conn.get());
    if (sockfd == -1)
        throw std::runtime_error("No DB socket is open.");
    struct sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    if (getpeername(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &len) ==
        -1)
    {
        throw std::system_error(
            errno, std::generic_category(), "Can't get server address info.");
    }

    // Set "port" and "hostaddr" if we're caching it.
    bool const remember_ip = config.contains("remember_ip")
        ? config.at("remember_ip").as_bool()
        : true;

    if (remember_ip)
    {
        config_.keywords.push_back("port");
        config_.keywords.push_back("hostaddr");
        std::string port;
        std::string hostaddr;

        if (addr.ss_family == AF_INET)
        {
            hostaddr.assign(INET_ADDRSTRLEN, '\0');
            struct sockaddr_in const& ainfo =
                reinterpret_cast<struct sockaddr_in&>(addr);
            port = std::to_string(ntohs(ainfo.sin_port));
            if (!inet_ntop(
                    AF_INET, &ainfo.sin_addr, &hostaddr[0], hostaddr.size()))
            {
                throw std::system_error(
                    errno,
                    std::generic_category(),
                    "Can't get IPv4 address string.");
            }
        }
        else if (addr.ss_family == AF_INET6)
        {
            hostaddr.assign(INET6_ADDRSTRLEN, '\0');
            struct sockaddr_in6 const& ainfo =
                reinterpret_cast<struct sockaddr_in6&>(addr);
            port = std::to_string(ntohs(ainfo.sin6_port));
            if (!inet_ntop(
                    AF_INET6, &ainfo.sin6_addr, &hostaddr[0], hostaddr.size()))
            {
                throw std::system_error(
                    errno,
                    std::generic_category(),
                    "Can't get IPv6 address string.");
            }
        }

        config_.values.push_back(port.c_str());
        config_.values.push_back(hostaddr.c_str());
    }
    std::unique_ptr<PQconninfoOption, void (*)(PQconninfoOption*)> connOptions(
        PQconninfo(conn.get()),
        [](PQconninfoOption* opts) { PQconninfoFree(opts); });
    if (!connOptions)
        throw std::runtime_error("Can't get DB connection options.");

    std::size_t nfields = 0;
    for (PQconninfoOption* option = connOptions.get();
         option->keyword != nullptr;
         ++option)
    {
        if (++nfields > maxFields)
        {
            std::stringstream ss;
            ss << "DB returned connection options with > " << maxFields
               << " fields.";
            throw std::runtime_error(ss.str());
        }

        if (!option->val ||
            (remember_ip &&
             (!strcmp(option->keyword, "hostaddr") ||
              !strcmp(option->keyword, "port"))))
        {
            continue;
        }

        if (strlen(option->keyword) > maxFieldSize ||
            strlen(option->val) > maxFieldSize)
        {
            std::stringstream ss;
            ss << "DB returned a connection option name or value with\n";
            ss << "excessive size (>" << maxFieldSize << " bytes).\n";
            ss << "option (possibly truncated): "
               << std::string_view(
                      option->keyword,
                      std::min(strlen(option->keyword), maxFieldSize))
               << '\n';
            ss << " value (possibly truncated): "
               << std::string_view(
                      option->val, std::min(strlen(option->val), maxFieldSize));
            throw std::runtime_error(ss.str());
        }
        config_.keywords.push_back(option->keyword);
        config_.values.push_back(option->val);
    }

    config_.keywordsIdx.reserve(config_.keywords.size() + 1);
    config_.valuesIdx.reserve(config_.values.size() + 1);
    for (std::size_t n = 0; n < config_.keywords.size(); ++n)
    {
        config_.keywordsIdx.push_back(config_.keywords[n].c_str());
        config_.valuesIdx.push_back(config_.values[n].c_str());
    }
    config_.keywordsIdx.push_back(nullptr);
    config_.valuesIdx.push_back(nullptr);

    if (config.contains("max_connections"))
        config_.max_connections = config.at("max_connections").as_uint64();
    std::size_t timeout;
    if (config.contains("timeout"))
        config_.timeout =
            std::chrono::seconds(config.at("timeout").as_uint64());
}

void
PgPool::setup()
{
    {
        std::stringstream ss;
        ss << "max_connections: " << config_.max_connections << ", "
           << "timeout: " << config_.timeout.count() << ", "
           << "connection params: ";
        bool first = true;
        for (std::size_t i = 0; i < config_.keywords.size(); ++i)
        {
            if (first)
                first = false;
            else
                ss << ", ";
            ss << config_.keywords[i] << ": "
               << (config_.keywords[i] == "password" ? "*" : config_.values[i]);
        }
        BOOST_LOG_TRIVIAL(debug) << ss.str();
    }
}

void
PgPool::onStop()
{
    std::lock_guard<std::mutex> lock(mutex_);
    stop_ = true;
    cond_.notify_all();
    idle_.clear();
    BOOST_LOG_TRIVIAL(info) << "stopped";
}

void
PgPool::idleSweeper()
{
    std::size_t before, after;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        before = idle_.size();
        if (config_.timeout != std::chrono::seconds(0))
        {
            auto const found =
                idle_.upper_bound(clock_type::now() - config_.timeout);
            for (auto it = idle_.begin(); it != found;)
            {
                it = idle_.erase(it);
                --connections_;
            }
        }
        after = idle_.size();
    }

    BOOST_LOG_TRIVIAL(info)
        << "Idle sweeper. connections: " << connections_
        << ". checked out: " << connections_ - after
        << ". idle before, after sweep: " << before << ", " << after;
}

std::unique_ptr<Pg>
PgPool::checkout()
{
    std::unique_ptr<Pg> ret;
    std::unique_lock<std::mutex> lock(mutex_);
    do
    {
        if (stop_)
            return {};

        // If there is a connection in the pool, return the most recent.
        if (idle_.size())
        {
            auto entry = idle_.rbegin();
            ret = std::move(entry->second);
            idle_.erase(std::next(entry).base());
        }
        // Otherwise, return a new connection unless over threshold.
        else if (connections_ < config_.max_connections)
        {
            ++connections_;
            ret = std::make_unique<Pg>(config_, stop_, mutex_);
        }
        // Otherwise, wait until a connection becomes available or we stop.
        else
        {
            BOOST_LOG_TRIVIAL(error) << "No database connections available.";
            cond_.wait(lock);
        }
    } while (!ret && !stop_);
    lock.unlock();

    return ret;
}

void
PgPool::checkin(std::unique_ptr<Pg>& pg)
{
    if (pg)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!stop_ && pg->clear())
        {
            idle_.emplace(clock_type::now(), std::move(pg));
        }
        else
        {
            --connections_;
            pg.reset();
        }
    }

    cond_.notify_all();
}

//-----------------------------------------------------------------------------

std::shared_ptr<PgPool>
make_PgPool(boost::json::object const& config)
{
    try
    {
        auto ret = std::make_shared<PgPool>(config);
        ret->setup();
        return ret;
    }
    catch (std::runtime_error& e)
    {
        boost::json::object configCopy = config;
        configCopy["database"] = "postgres";
        auto ret = std::make_shared<PgPool>(configCopy);
        ret->setup();
        PgQuery pgQuery{ret};
        std::string query = "CREATE DATABASE " +
            std::string{config.at("database").as_string().c_str()};
        pgQuery(query.c_str());
        ret = std::make_shared<PgPool>(config);
        ret->setup();
        return ret;
    }
}

//-----------------------------------------------------------------------------

/** Postgres Schema Management
 *
 * The postgres schema has several properties to facilitate
 * consistent deployments, including upgrades. It is not recommended to
 * upgrade the schema concurrently.
 *
 * Initial deployment should be against a completely fresh database. The
 * postgres user must have the CREATE TABLE privilege.
 *
 * With postgres configured, the first step is to apply the version_query
 * schema and consume the results. This script returns the currently
 * installed schema version, if configured, or 0 if not. It is idempotent.
 *
 * If the version installed on the database is equal to the
 * LATEST_SCHEMA_VERSION, then no action should take place.
 *
 * If the version on the database is 0, then the entire latest schema
 * should be deployed with the applySchema() function.
 * Each version that is developed is fully
 * represented in the full_schemata array with each version equal to the
 * text in the array's index position. For example, index position 1
 * contains the full schema version 1. Position 2 contains schema version 2.
 * Index 0 should never be referenced and its value only a placeholder.
 * If a fresh installation is aborted, then subsequent fresh installations
 * should install the same version previously attempted, even if there
 * exists a newer version. The initSchema() function performs this task.
 * Therefore, previous schema versions should remain in the array
 * without modification as new versions are developed and placed after them.
 * Once the schema is succesffuly deployed, applySchema() persists the
 * schema version to the database.
 *
 * If the current version of the database is greater than 0, then it means
 * that a previous schema version is already present. In this case, the database
 * schema needs to be updated incrementally for each subsequent version.
 * Again, applySchema() is used to upgrade the schema. Schema upgrades are
 * in the upgrade_schemata array. Each entry by index position represents
 * the database schema version from which the upgrade begins. Each upgrade
 * sets the database to the next version. Schema upgrades can only safely
 * happen from one version to the next. To upgrade several versions of schema,
 * upgrade incrementally for each version that separates the current from the
 * latest. For example, to upgrade from version 5 to version 6 of the schema,
 * use upgrade_schemata[5]. To upgrade from version 1 to version 4, use
 * upgrade_schemata[1], upgrade_schemata[2], and upgrade_schemata[3] in
 * sequence.
 *
 * To upgrade the schema past version 1, the following variables must be
 * updated:
 * 1) LATEST_SCHEMA_VERSION must be set to the new version.
 * 2) A new entry must be placed at the end of the full_schemata array. This
 *    entry should have the entire schema so that fresh installations can
 *    be performed with it. The index position must be equal to the
 *    LATEST_SCHEMA_VERSION.
 * 3) A new entry must be placed at the end of the upgrade_schemata array.
 *    This entry should only contain commands to upgrade the schema from
 *    the immediately previous version to the new version.
 *
 * It is up to the developer to ensure that all schema commands are idempotent.
 * This protects against 2 things:
 * 1) Resuming schema installation after a problem.
 * 2) Concurrent schema updates from multiple processes.
 *
 * There are several things that must considered for upgrading existing
 * schemata to avoid stability and performance problems. Some examples and
 * suggestions follow.
 * - Schema changes such as creating new columns and indices can consume
 *   a lot of time. Therefore, before such changes, a separate script should
 *   be executed by the user to perform the schema upgrade prior to restarting
 *   rippled.
 * - Stored functions cannot be dropped while being accessed. Also,
 *   dropping stored functions can be ambiguous if multiple functions with
 *   the same name but different signatures exist. Further, stored function
 *   behavior from one schema version to the other would likely be handled
 *   differently by rippled. In this case, it is likely that the functions
 *   themselves should be versioned such as by appending a number to the
 *   end of the name (abcf becomes abcf_2, abcf_3, etc.)
 *
 * Essentially, each schema upgrade will have its own factors to impact
 * service availability and function.
 */

#define LATEST_SCHEMA_VERSION 1

char const* version_query = R"(
CREATE TABLE IF NOT EXISTS version (version int NOT NULL,
    fresh_pending int NOT NULL);

-- Version 0 means that no schema has been fully deployed.
DO $$
BEGIN
    IF NOT EXISTS (SELECT 1 FROM version) THEN
    INSERT INTO version VALUES (0, 0);
END IF;
END $$;

-- Function to set the schema version. _in_pending should only be set to
-- non-zero prior to an attempt to initialize the schema from scratch.
-- After successful initialization, this should set to 0.
-- _in_version should be set to the version of schema that has been applied
-- once successful application has occurred.
CREATE OR REPLACE FUNCTION set_schema_version (
    _in_version int,
    _in_pending int
) RETURNS void AS $$
DECLARE
    _current_version int;
BEGIN
    IF _in_version IS NULL OR _in_pending IS NULL THEN RETURN; END IF;
    IF EXISTS (SELECT 1 FROM version) THEN DELETE FROM version; END IF;
    INSERT INTO version VALUES (_in_version, _in_pending);
    RETURN;
END;
$$ LANGUAGE plpgsql;

-- PQexec() returns the output of the last statement in its response.
SELECT * FROM version;
)";

std::array<char const*, LATEST_SCHEMA_VERSION + 1> full_schemata = {
    // version 0:
    "There is no such thing as schema version 0."

    // version 1:
    ,
    R"(
-- Table to store ledger headers.
CREATE TABLE IF NOT EXISTS ledgers (
    ledger_seq        bigint PRIMARY KEY,
    ledger_hash       bytea  NOT NULL,
    prev_hash         bytea  NOT NULL,
    total_coins       bigint NOT NULL,
    closing_time      bigint NOT NULL,
    prev_closing_time bigint NOT NULL,
    close_time_res    bigint NOT NULL,
    close_flags       bigint NOT NULL,
    account_set_hash  bytea  NOT NULL,
    trans_set_hash    bytea  NOT NULL
);


CREATE TABLE IF NOT EXISTS objects (
    key bytea NOT NULL,
    ledger_seq bigint NOT NULL REFERENCES ledgers ON DELETE CASCADE,
    object bytea
) PARTITION BY RANGE (ledger_seq);

CREATE INDEX objects_idx ON objects USING btree(key,ledger_seq);

create table if not exists objects1 partition of objects for values from (0) to (10000000);
create table if not exists objects2 partition of objects for values from (10000000) to (20000000);
create table if not exists objects3 partition of objects for values from (20000000) to (30000000);
create table if not exists objects4 partition of objects for values from (30000000) to (40000000);
create table if not exists objects5 partition of objects for values from (40000000) to (50000000);
create table if not exists objects6 partition of objects for values from (50000000) to (60000000);
create table if not exists objects7 partition of objects for values from (60000000) to (70000000);


-- Index for lookups by ledger hash.
CREATE INDEX IF NOT EXISTS ledgers_ledger_hash_idx ON ledgers
    USING hash (ledger_hash);

-- Transactions table. Deletes from the ledger table
-- cascade here based on ledger_seq.
CREATE TABLE IF NOT EXISTS transactions (
    hash bytea NOT NULL,
    ledger_seq bigint NOT NULL REFERENCES ledgers ON DELETE CASCADE,
    transaction bytea NOT NULL,
    metadata bytea NOT NULL
) PARTITION BY RANGE(ledger_seq);
create table if not exists transactions1 partition of transactions for values from (0) to (10000000);
create table if not exists transactions2 partition of transactions for values from (10000000) to (20000000);
create table if not exists transactions3 partition of transactions for values from (20000000) to (30000000);
create table if not exists transactions4 partition of transactions for values from (30000000) to (40000000);
create table if not exists transactions5 partition of transactions for values from (40000000) to (50000000);
create table if not exists transactions6 partition of transactions for values from (50000000) to (60000000);
create table if not exists transactions7 partition of transactions for values from (60000000) to (70000000);

create index if not exists tx_by_hash on transactions using hash (hash);
create index if not exists tx_by_lgr_seq on transactions using hash (ledger_seq);

-- Table that maps accounts to transactions affecting them. Deletes from the
-- ledger table cascade here based on ledger_seq.
CREATE TABLE IF NOT EXISTS account_transactions (
    account           bytea  NOT NULL,
    ledger_seq        bigint NOT NULL REFERENCES ledgers ON DELETE CASCADE,
    transaction_index bigint NOT NULL,
    hash bytea NOT NULL,
    PRIMARY KEY (account, ledger_seq, transaction_index, hash)
) PARTITION BY RANGE (ledger_seq);
create table if not exists account_transactions1 partition of account_transactions for values from (0) to (10000000);
create table if not exists account_transactions2 partition of account_transactions for values from (10000000) to (20000000);
create table if not exists account_transactions3 partition of account_transactions for values from (20000000) to (30000000);
create table if not exists account_transactions4 partition of account_transactions for values from (30000000) to (40000000);
create table if not exists account_transactions5 partition of account_transactions for values from (40000000) to (50000000);
create table if not exists account_transactions6 partition of account_transactions for values from (50000000) to (60000000);
create table if not exists account_transactions7 partition of account_transactions for values from (60000000) to (70000000);


CREATE TABLE IF NOT EXISTS keys (
    ledger_seq bigint NOT NULL, 
    key bytea NOT NULL
);

CREATE INDEX key_idx ON keys USING btree(ledger_seq, key);

-- account_tx() RPC helper. From the rippled reporting process, only the
-- parameters without defaults are required. For the parameters with
-- defaults, validation should be done by rippled, such as:
-- _in_account_id should be a valid xrp base58 address.
-- _in_forward either true or false according to the published api
-- _in_limit should be validated and not simply passed through from
-- client.
--
-- For _in_ledger_index_min and _in_ledger_index_max, if passed in the
-- request, verify that their type is int and pass through as is.
-- For _ledger_hash, verify and convert from hex length 32 bytes and
-- prepend with \x (\\x C++).
--
-- For _in_ledger_index, if the input type is integer, then pass through
-- as is. If the type is string and contents = validated, then do not
-- set _in_ledger_index. Instead set _in_invalidated to TRUE.
--
-- There is no need for rippled to do any type of lookup on max/min
-- ledger range, lookup of hash, or the like. This functions does those
-- things, including error responses if bad input. Only the above must
-- be done to set the correct search range.
--
-- If a marker is present in the request, verify the members 'ledger'
-- and 'seq' are integers and they correspond to _in_marker_seq
-- _in_marker_index.
-- To reiterate:
-- JSON input field 'ledger' corresponds to _in_marker_seq
-- JSON input field 'seq' corresponds to _in_marker_index
CREATE OR REPLACE FUNCTION account_tx(
        _in_account_id bytea,
        _in_limit bigint,
        _in_marker_seq bigint DEFAULT NULL::bigint,
        _in_marker_index bigint DEFAULT NULL::bigint)
RETURNS jsonb
AS $$
DECLARE
    _min          bigint;
    _max          bigint;
    _marker       bool;
    _between_min  bigint;
    _between_max  bigint;
    _sql          text;
    _cursor       refcursor;
    _result       jsonb;
    _record       record;
    _tally        bigint     := 0;
    _ret_marker   jsonb;
    _transactions jsonb[]    := '{}';
BEGIN
    _min := min_ledger();
    _max := max_ledger();
    IF _in_marker_seq IS NOT NULL OR _in_marker_index IS NOT NULL THEN
        _marker := TRUE;
        IF _in_marker_seq IS NULL OR _in_marker_index IS NULL THEN
            -- The rippled implementation returns no transaction results
            -- if either of these values are missing.
            _between_min := 0;
            _between_max := 0;
        ELSE
            _between_min := _min;
            _between_max := _in_marker_seq;
        END IF;
    ELSE
        _marker := FALSE;
        _between_min := _min;
        _between_max := _max;
    END IF;


    _sql := format('SELECT hash, ledger_seq, transaction_index FROM account_transactions WHERE account = $1
        AND ledger_seq BETWEEN $2 AND $3 ORDER BY ledger_seq DESC, transaction_index DESC');

    OPEN _cursor FOR EXECUTE _sql USING _in_account_id, _between_min, _between_max;
    LOOP
        FETCH _cursor INTO _record;
        IF _record IS NULL THEN EXIT; END IF;
        IF _marker IS TRUE THEN
            IF _in_marker_seq = _record.ledger_seq THEN
                IF _in_marker_index < _record.transaction_index THEN
                    CONTINUE;
                END IF;
            END IF;
            _marker := FALSE;
        END IF;
        _tally := _tally + 1;
        IF _tally > _in_limit THEN
            _ret_marker := jsonb_build_object(
                'ledger_sequence', _record.ledger_seq,
                'transaction_index', _record.transaction_index);
            EXIT;
        END IF;

        -- Is the transaction index in the tx object?
        _transactions := _transactions || jsonb_build_object('hash',_record.hash);
    END LOOP;
    CLOSE _cursor;

    _result := jsonb_build_object('ledger_index_min', _min,
        'ledger_index_max', _max,
        'transactions', _transactions);
    IF _ret_marker IS NOT NULL THEN
        _result := _result || jsonb_build_object('cursor', _ret_marker);
    END IF;
    RETURN _result;
END;
$$ LANGUAGE plpgsql;

-- Avoid inadvertent administrative tampering with committed data.
CREATE OR REPLACE RULE ledgers_update_protect AS ON UPDATE TO
    ledgers DO INSTEAD NOTHING;
CREATE OR REPLACE RULE transactions_update_protect AS ON UPDATE TO
    transactions DO INSTEAD NOTHING;
CREATE OR REPLACE RULE account_transactions_update_protect AS ON UPDATE TO
    account_transactions DO INSTEAD NOTHING;
CREATE OR REPLACE RULE objects_update_protect AS ON UPDATE TO
    objects DO INSTEAD NOTHING;
CREATE OR REPLACE RULE books_update_protect AS ON UPDATE TO
    books DO INSTEAD NOTHING;


-- Return the earliest ledger sequence intended for range operations
-- that protect the bottom of the range from deletion. Return NULL if empty.
CREATE OR REPLACE FUNCTION min_ledger () RETURNS bigint AS $$
DECLARE
    _min_seq bigint := (SELECT ledger_seq from min_seq);
BEGIN
    IF _min_seq IS NULL THEN
        RETURN (SELECT ledger_seq FROM ledgers ORDER BY ledger_seq ASC LIMIT 1);
    ELSE
        RETURN _min_seq;
    END IF;
END;
$$ LANGUAGE plpgsql;

-- Return the latest ledger sequence in the database, or NULL if empty.
CREATE OR REPLACE FUNCTION max_ledger () RETURNS bigint AS $$
BEGIN
    RETURN (SELECT ledger_seq FROM ledgers ORDER BY ledger_seq DESC LIMIT 1);
END;
$$ LANGUAGE plpgsql;


-- Trigger prior to insert on ledgers table. Validates length of hash fields.
-- Verifies ancestry based on ledger_hash & prev_hash as follows:
-- 1) If ledgers is empty, allows insert.
-- 2) For each new row, check for previous and later ledgers by a single
--    sequence. For each that exist, confirm ancestry based on hashes.
-- 3) Disallow inserts with no prior or next ledger by sequence if any
--    ledgers currently exist. This disallows gaps to be introduced by
--    way of inserting.
CREATE OR REPLACE FUNCTION insert_ancestry() RETURNS TRIGGER AS $$
DECLARE
    _parent bytea;
    _child  bytea;
BEGIN
    IF length(NEW.ledger_hash) != 32 OR length(NEW.prev_hash) != 32 THEN
        RAISE 'ledger_hash and prev_hash must each be 32 bytes: %', NEW;
    END IF;

    IF (SELECT ledger_hash
          FROM ledgers
         ORDER BY ledger_seq DESC
         LIMIT 1) = NEW.prev_hash THEN RETURN NEW; END IF;

    IF NOT EXISTS (SELECT 1 FROM LEDGERS) THEN RETURN NEW; END IF;

    _parent := (SELECT ledger_hash
                  FROM ledgers
                 WHERE ledger_seq = NEW.ledger_seq - 1);
    _child  := (SELECT prev_hash
                  FROM ledgers
                 WHERE ledger_seq = NEW.ledger_seq + 1);
    IF _parent IS NULL AND _child IS NULL THEN
        RAISE 'Ledger Ancestry error: orphan.';
    END IF;
    IF _parent != NEW.prev_hash THEN
        RAISE 'Ledger Ancestry error: bad parent.';
    END IF;
    IF _child != NEW.ledger_hash THEN
        RAISE 'Ledger Ancestry error: bad child.';
    END IF;

    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
CREATE TRIGGER verify_ancestry BEFORE INSERT OR UPDATE on ledgers
    FOR EACH ROW EXECUTE PROCEDURE insert_ancestry();

-- Trigger function prior to delete on ledgers table. Disallow gaps from
-- forming. Do not allow deletions if both the previous and next ledgers
-- are present. In other words, only allow either the least or greatest
-- to be deleted.
CREATE OR REPLACE FUNCTION delete_ancestry () RETURNS TRIGGER AS $$
BEGIN
    IF EXISTS (SELECT 1
                 FROM ledgers
                WHERE ledger_seq = OLD.ledger_seq + 1)
            AND EXISTS (SELECT 1
                          FROM ledgers
                         WHERE ledger_seq = OLD.ledger_seq - 1) THEN
        RAISE 'Ledger Ancestry error: Can only delete the least or greatest '
              'ledger.';
    END IF;
    RETURN OLD;
END;
$$ LANGUAGE plpgsql;

-- Track the minimum sequence that should be used for ranged queries
-- with protection against deletion during the query. This should
-- be updated before calling online_delete() to not block deleting that
-- range.
CREATE TABLE IF NOT EXISTS min_seq (
    ledger_seq bigint NOT NULL
);

-- Set the minimum sequence for use in ranged queries with protection
-- against deletion greater than or equal to the input parameter. This
-- should be called prior to online_delete() with the same parameter
-- value so that online_delete() is not blocked by range queries
-- that are protected against concurrent deletion of the ledger at
-- the bottom of the range. This function needs to be called from a
-- separate transaction from that which executes online_delete().
CREATE OR REPLACE FUNCTION prepare_delete (
    _in_last_rotated bigint
) RETURNS void AS $$
BEGIN
    IF EXISTS (SELECT 1 FROM min_seq) THEN
        DELETE FROM min_seq;
    END IF;
    INSERT INTO min_seq VALUES (_in_last_rotated + 1);
END;
$$ LANGUAGE plpgsql;

-- Function to delete old data. All data belonging to ledgers prior to and
-- equal to the _in_seq parameter will be deleted. This should be
-- called with the input parameter equivalent to the value of lastRotated
-- in rippled's online_delete routine.
CREATE OR REPLACE FUNCTION online_delete (
    _in_seq bigint
) RETURNS void AS $$
BEGIN
    DELETE FROM LEDGERS WHERE ledger_seq <= _in_seq;
END;
$$ LANGUAGE plpgsql;

-- Function to delete data from the top of the ledger range. Delete
-- everything greater than the input parameter.
-- It doesn't do a normal range delete because of the trigger protecting
-- deletions causing gaps. Instead, it walks back from the greatest ledger.
CREATE OR REPLACE FUNCTION delete_above (
    _in_seq bigint
) RETURNS void AS $$
DECLARE
    _max_seq bigint := max_ledger();
    _i bigint := _max_seq;
BEGIN
    IF _max_seq IS NULL THEN RETURN; END IF;
    LOOP
        IF _i <= _in_seq THEN RETURN; END IF;
        EXECUTE 'DELETE FROM ledgers WHERE ledger_seq = $1' USING _i;
        _i := _i - 1;
    END LOOP;
END;
$$ LANGUAGE plpgsql;

-- Verify correct ancestry of ledgers in database:
-- Table to persist last-confirmed latest ledger with proper ancestry.
CREATE TABLE IF NOT EXISTS ancestry_verified (
    ledger_seq bigint NOT NULL
);

-- Function to verify ancestry of ledgers based on ledger_hash and prev_hash.
-- Upon failure, returns ledger sequence failing ancestry check.
-- Otherwise, returns NULL.
-- _in_full: If TRUE, verify entire table. Else verify starting from
--           value in ancestry_verfied table. If no value, then start
--           from lowest ledger.
-- _in_persist: If TRUE, persist the latest ledger with correct ancestry.
--              If an exception was raised because of failure, persist
--              the latest ledger prior to that which failed.
-- _in_min: If set and _in_full is not true, the starting ledger from which
--          to verify.
-- _in_max: If set and _in_full is not true, the latest ledger to verify.
CREATE OR REPLACE FUNCTION check_ancestry (
    _in_full    bool = FALSE,
    _in_persist bool = TRUE,
    _in_min      bigint = NULL,
    _in_max      bigint = NULL
) RETURNS bigint AS $$
DECLARE
    _min                 bigint;
    _max                 bigint;
    _last_verified       bigint;
    _parent          ledgers;
    _current         ledgers;
    _cursor        refcursor;
BEGIN
    IF _in_full IS TRUE AND
            (_in_min IS NOT NULL) OR (_in_max IS NOT NULL) THEN
        RAISE 'Cannot specify manual range and do full check.';
    END IF;

    IF _in_min IS NOT NULL THEN
        _min := _in_min;
    ELSIF _in_full IS NOT TRUE THEN
        _last_verified := (SELECT ledger_seq FROM ancestry_verified);
        IF _last_verified IS NULL THEN
            _min := min_ledger();
        ELSE
            _min := _last_verified + 1;
        END IF;
    ELSE
        _min := min_ledger();
    END IF;
    EXECUTE 'SELECT * FROM ledgers WHERE ledger_seq = $1'
        INTO _parent USING _min - 1;
    IF _last_verified IS NOT NULL AND _parent IS NULL THEN
        RAISE 'Verified ledger % doesn''t exist.', _last_verified;
    END IF;

    IF _in_max IS NOT NULL THEN
        _max := _in_max;
    ELSE
        _max := max_ledger();
    END IF;

    OPEN _cursor FOR EXECUTE 'SELECT *
                                FROM ledgers
                               WHERE ledger_seq BETWEEN $1 AND $2
                               ORDER BY ledger_seq ASC'
                               USING _min, _max;
    LOOP
        FETCH _cursor INTO _current;
        IF _current IS NULL THEN EXIT; END IF;
        IF _parent IS NOT NULL THEN
            IF _current.prev_hash != _parent.ledger_hash THEN
                CLOSE _cursor;
                RETURN _current.ledger_seq;
                RAISE 'Ledger ancestry failure current, parent:% %',
                    _current, _parent;
            END IF;
        END IF;
        _parent := _current;
    END LOOP;
    CLOSE _cursor;

    IF _in_persist IS TRUE AND _parent IS NOT NULL THEN
        DELETE FROM ancestry_verified;
        INSERT INTO ancestry_verified VALUES (_parent.ledger_seq);
    END IF;

    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

-- Return number of whole seconds since the latest ledger was inserted, based
-- on ledger close time (not wall clock) of the insert.
-- Note that ledgers.closing_time is number of seconds since the XRP
-- epoch, which is 01/01/2000 00:00:00. This in turn is 946684800 seconds
-- after the UNIX epoch. This conforms to the "age" field in the
-- server_info RPC call.
CREATE OR REPLACE FUNCTION age () RETURNS bigint AS $$
BEGIN
    RETURN (EXTRACT(EPOCH FROM (now())) -
        (946684800 + (SELECT closing_time
                        FROM ledgers
                       ORDER BY ledger_seq DESC
                       LIMIT 1)))::bigint;
END;
$$ LANGUAGE plpgsql;

-- Return range of ledgers, or empty if none. This conforms to the
-- "complete_ledgers" field of the server_info RPC call. Note
-- that ledger gaps are prevented for reporting mode so the range
-- is simply the set between the least and greatest ledgers.
CREATE OR REPLACE FUNCTION complete_ledgers () RETURNS text AS $$
DECLARE
    _min bigint := min_ledger();
    _max bigint := max_ledger();
BEGIN
    IF _min IS NULL THEN RETURN 'empty'; END IF;
    IF _min = _max THEN RETURN _min; END IF;
    RETURN _min || '-' || _max;
END;
$$ LANGUAGE plpgsql;

)"

    // version 2:
    //  , R"(Full idempotent text of schema version 2)"

    // version 3:
    //  , R"(Full idempotent text of schema version 3)"

    // version 4:
    //  , R"(Full idempotent text of schema version 4)"

    //  ...

    // version n:
    //  , R"(Full idempotent text of schema version n)"
};

std::array<char const*, LATEST_SCHEMA_VERSION> upgrade_schemata = {
    // upgrade from version 0:
    "There is no upgrade path from version 0. Instead, install "
    "from full_schemata."
    // upgrade from version 1 to 2:
    //, R"(Text to idempotently upgrade from version 1 to 2)"
    // upgrade from version 2 to 3:
    //, R"(Text to idempotently upgrade from version 2 to 3)"
    // upgrade from version 3 to 4:
    //, R"(Text to idempotently upgrade from version 3 to 4)"
    // ...
    // upgrade from version n-1 to n:
    //, R"(Text to idempotently upgrade from version n-1 to n)"
};

/** Apply schema to postgres.
 *
 * The schema text should contain idempotent SQL & plpgSQL statements.
 * Once completed, the version of the schema will be persisted.
 *
 * Throws upon error.
 *
 * @param pool Postgres connection pool manager.
 * @param schema SQL commands separated by semi-colon.
 * @param currentVersion The current version of the schema on the database.
 * @param schemaVersion The version that will be in place once the schema
 *        has been applied.
 */
void
applySchema(
    std::shared_ptr<PgPool> const& pool,
    char const* schema,
    std::uint32_t currentVersion,
    std::uint32_t schemaVersion)
{
    if (currentVersion != 0 && schemaVersion != currentVersion + 1)
    {
        assert(false);
        std::stringstream ss;
        ss << "Schema upgrade versions past initial deployment must increase "
              "monotonically. Versions: current, target: "
           << currentVersion << ", " << schemaVersion;
        throw std::runtime_error(ss.str());
    }

    auto res = PgQuery(pool)({schema, {}});
    if (!res)
    {
        std::stringstream ss;
        ss << "Error applying schema from version " << currentVersion << "to "
           << schemaVersion << ": " << res.msg();
        throw std::runtime_error(ss.str());
    }

    auto cmd = boost::format(R"(SELECT set_schema_version(%u, 0))");
    res = PgQuery(pool)({boost::str(cmd % schemaVersion).c_str(), {}});
    if (!res)
    {
        std::stringstream ss;
        ss << "Error setting schema version from " << currentVersion << " to "
           << schemaVersion << ": " << res.msg();
        throw std::runtime_error(ss.str());
    }
}

void
initSchema(std::shared_ptr<PgPool> const& pool)
{
    // Figure out what schema version, if any, is already installed.
    auto res = PgQuery(pool)({version_query, {}});
    if (!res)
    {
        std::stringstream ss;
        ss << "Error getting database schema version: " << res.msg();
        throw std::runtime_error(ss.str());
    }
    std::uint32_t currentSchemaVersion = res.asInt();
    std::uint32_t const pendingSchemaVersion = res.asInt(0, 1);

    // Nothing to do if we are on the latest schema;
    if (currentSchemaVersion == LATEST_SCHEMA_VERSION)
        return;

    if (currentSchemaVersion == 0)
    {
        // If a fresh install has not been completed, then re-attempt
        // the install of the same schema version.
        std::uint32_t const freshVersion =
            pendingSchemaVersion ? pendingSchemaVersion : LATEST_SCHEMA_VERSION;
        // Persist that we are attempting a fresh install to the latest version.
        // This protects against corruption in an aborted install that is
        // followed by a fresh installation attempt with a new schema.
        auto cmd = boost::format(R"(SELECT set_schema_version(0, %u))");
        res = PgQuery(pool)({boost::str(cmd % freshVersion).c_str(), {}});
        if (!res)
        {
            std::stringstream ss;
            ss << "Error setting schema version from " << currentSchemaVersion
               << " to " << freshVersion << ": " << res.msg();
            throw std::runtime_error(ss.str());
        }

        // Install the full latest schema.
        applySchema(
            pool,
            full_schemata[freshVersion],
            currentSchemaVersion,
            freshVersion);
        currentSchemaVersion = freshVersion;
    }

    // Incrementally upgrade one version at a time until latest.
    for (; currentSchemaVersion < LATEST_SCHEMA_VERSION; ++currentSchemaVersion)
    {
        applySchema(
            pool,
            upgrade_schemata[currentSchemaVersion],
            currentSchemaVersion,
            currentSchemaVersion + 1);
    }
}

// Load the ledger info for the specified ledger/s from the database
// @param whichLedger specifies the ledger to load via ledger sequence, ledger
// hash, a range of ledgers, or std::monostate (which loads the most recent)
// @return LedgerInfo
std::optional<ripple::LedgerInfo>
getLedger(
    std::variant<std::monostate, ripple::uint256, uint32_t> const& whichLedger,
    std::shared_ptr<PgPool>& pgPool)
{
    ripple::LedgerInfo lgrInfo;
    std::stringstream sql;
    sql << "SELECT ledger_hash, prev_hash, account_set_hash, trans_set_hash, "
           "total_coins, closing_time, prev_closing_time, close_time_res, "
           "close_flags, ledger_seq FROM ledgers ";

    uint32_t expNumResults = 1;

    if (auto ledgerSeq = std::get_if<uint32_t>(&whichLedger))
    {
        sql << "WHERE ledger_seq = " + std::to_string(*ledgerSeq);
    }
    else if (auto ledgerHash = std::get_if<ripple::uint256>(&whichLedger))
    {
        sql << ("WHERE ledger_hash = \'\\x" + strHex(*ledgerHash) + "\'");
    }
    else
    {
        sql << ("ORDER BY ledger_seq desc LIMIT 1");
    }
    sql << ";";

    BOOST_LOG_TRIVIAL(trace) << __func__ << " : sql = " << sql.str();

    auto res = PgQuery(pgPool)(sql.str().data());
    if (!res)
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__ << " : Postgres response is null - sql = " << sql.str();
        assert(false);
        return {};
    }
    else if (res.status() != PGRES_TUPLES_OK)
    {
        BOOST_LOG_TRIVIAL(error) << __func__
                                 << " : Postgres response should have been "
                                    "PGRES_TUPLES_OK but instead was "
                                 << res.status() << " - msg  = " << res.msg()
                                 << " - sql = " << sql.str();
        assert(false);
        return {};
    }

    BOOST_LOG_TRIVIAL(trace)
        << __func__ << " Postgres result msg  : " << res.msg();

    if (res.isNull() || res.ntuples() == 0)
    {
        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " : Ledger not found. sql = " << sql.str();
        return {};
    }
    else if (res.ntuples() > 0)
    {
        if (res.nfields() != 10)
        {
            BOOST_LOG_TRIVIAL(error)
                << __func__
                << " : Wrong number of fields in Postgres "
                   "response. Expected 10, but got "
                << res.nfields() << " . sql = " << sql.str();
            assert(false);
            return {};
        }
    }

    char const* hash = res.c_str(0, 0);
    char const* prevHash = res.c_str(0, 1);
    char const* accountHash = res.c_str(0, 2);
    char const* txHash = res.c_str(0, 3);
    std::int64_t totalCoins = res.asBigInt(0, 4);
    std::int64_t closeTime = res.asBigInt(0, 5);
    std::int64_t parentCloseTime = res.asBigInt(0, 6);
    std::int64_t closeTimeRes = res.asBigInt(0, 7);
    std::int64_t closeFlags = res.asBigInt(0, 8);
    std::int64_t ledgerSeq = res.asBigInt(0, 9);

    BOOST_LOG_TRIVIAL(trace)
        << __func__ << " - Postgres response = " << hash << " , " << prevHash
        << " , " << accountHash << " , " << txHash << " , " << totalCoins
        << ", " << closeTime << ", " << parentCloseTime << ", " << closeTimeRes
        << ", " << closeFlags << ", " << ledgerSeq << " - sql = " << sql.str();
    BOOST_LOG_TRIVIAL(debug)
        << __func__
        << " - Successfully fetched ledger with sequence = " << ledgerSeq
        << " from Postgres";

    using time_point = ripple::NetClock::time_point;
    using duration = ripple::NetClock::duration;

    ripple::LedgerInfo info;
    if (!info.parentHash.parseHex(prevHash + 2))
        assert(false);
    if (!info.txHash.parseHex(txHash + 2))
        assert(false);
    if (!info.accountHash.parseHex(accountHash + 2))
        assert(false);
    info.drops = totalCoins;
    info.closeTime = time_point{duration{closeTime}};
    info.parentCloseTime = time_point{duration{parentCloseTime}};
    info.closeFlags = closeFlags;
    info.closeTimeResolution = duration{closeTimeRes};
    info.seq = ledgerSeq;
    if (!info.hash.parseHex(hash + 2))
        assert(false);
    info.validated = true;

    return info;
}

