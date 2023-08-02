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

#pragma once

#include <backend/BackendInterface.h>
#include <config/Config.h>
#include <etl/ETLHelpers.h>
#include <log/Logger.h>
#include <subscriptions/SubscriptionManager.h>

#include <ripple/proto/org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>
#include <boost/asio.hpp>
#include <grpcpp/grpcpp.h>

class Source;
class ProbingSource;
class SubscriptionManager;

/**
 * @brief This class is used to manage connections to transaction processing processes
 *
 * This class spawns a listener for each etl source, which listens to messages on the ledgers stream (to keep track of
 * which ledgers have been validated by the network, and the range of ledgers each etl source has). This class also
 * allows requests for ledger data to be load balanced across all possible ETL sources.
 */
class LoadBalancer
{
public:
    using RawLedgerObjectType = org::xrpl::rpc::v1::RawLedgerObject;
    using GetLedgerResponseType = org::xrpl::rpc::v1::GetLedgerResponse;
    using OptionalGetLedgerResponseType = std::optional<GetLedgerResponseType>;

private:
    clio::Logger log_{"ETL"};
    std::vector<std::unique_ptr<Source>> sources_;
    std::uint32_t downloadRanges_ = 16;

public:
    /**
     * @brief Create an instance of the load balancer
     *
     * @param config The configuration to use
     * @param ioContext io context to run on
     * @param backend BackendInterface implementation
     * @param subscriptions Subscription manager
     * @param nwvl The network validated ledgers datastructure
     */
    LoadBalancer(
        clio::Config const& config,
        boost::asio::io_context& ioContext,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> nwvl);

    static std::shared_ptr<LoadBalancer>
    make_LoadBalancer(
        clio::Config const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> validatedLedgers);

    static std::unique_ptr<Source>
    make_Source(
        clio::Config const& config,
        boost::asio::io_context& ioContext,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> networkValidatedLedgers,
        LoadBalancer& balancer);

    ~LoadBalancer()
    {
        sources_.clear();
    }

    /**
     * @brief Load the initial ledger, writing data to the queue
     *
     * @param sequence sequence of ledger to download
     */
    std::pair<std::vector<std::string>, bool>
    loadInitialLedger(uint32_t sequence, bool cacheOnly = false);

    /**
     * @brief Fetch data for a specific ledger
     *
     * This function will continuously try to fetch data for the specified ledger until the fetch succeeds, the ledger
     * is found in the database, or the server is shutting down.
     *
     * @param ledgerSequence sequence of ledger to fetch data for
     * @param getObjects if true, fetch diff between specified ledger and previous
     * @return the extracted data, if extraction was successful. If the ledger was found in the database or the server
     * is shutting down, the optional will be empty
     */
    OptionalGetLedgerResponseType
    fetchLedger(uint32_t ledgerSequence, bool getObjects, bool getObjectNeighbors);

    /**
     * @brief Determine whether messages received on the transactions_proposed stream should be forwarded to subscribing
     * clients.
     *
     * The server subscribes to transactions_proposed on multiple Sources, yet only forwards messages from one source at
     * any given time (to avoid sending duplicate messages to clients).
     *
     * @param in Source in question
     * @return true if messages should be forwarded
     */
    bool
    shouldPropagateTxnStream(Source* in) const;

    /**
     * @return JSON representation of the state of this load balancer
     */
    boost::json::value
    toJson() const;

    /**
     * @brief Forward a JSON RPC request to a randomly selected rippled node
     *
     * @param request JSON-RPC request
     * @return response received from rippled node
     */
    std::optional<boost::json::object>
    forwardToRippled(boost::json::object const& request, std::string const& clientIp, boost::asio::yield_context yield)
        const;

private:
    /**
     * @brief Execute a function on a randomly selected source
     *
     * @note f is a function that takes an Source as an argument and returns a bool.
     * Attempt to execute f for one randomly chosen Source that has the specified ledger. If f returns false, another
     * randomly chosen Source is used. The process repeats until f returns true.
     *
     * @param f function to execute. This function takes the ETL source as an argument, and returns a bool.
     * @param ledgerSequence f is executed for each Source that has this ledger
     * @return true if f was eventually executed successfully. false if the ledger was found in the database or the
     * server is shutting down
     */
    template <class Func>
    bool
    execute(Func f, uint32_t ledgerSequence);
};
