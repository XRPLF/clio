#ifndef RIPPLE_APP_REPORTING_ETLSOURCE_H_INCLUDED
#define RIPPLE_APP_REPORTING_ETLSOURCE_H_INCLUDED

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <backend/BackendInterface.h>
#include <subscriptions/SubscriptionManager.h>

#include "org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h"
#include <etl/ETLHelpers.h>
#include <grpcpp/grpcpp.h>

class ETLLoadBalancer;
class SubscriptionManager;

/// This class manages a connection to a single ETL source. This is almost
/// always a rippled node, but really could be another reporting node. This
/// class subscribes to the ledgers and transactions_proposed streams of the
/// associated rippled node, and keeps track of which ledgers the rippled node
/// has. This class also has methods for extracting said ledgers. Lastly this
/// class forwards transactions received on the transactions_proposed streams to
/// any subscribers.

class ETLSource
{
public:
    virtual bool
    isConnected() const = 0;

    virtual boost::json::object
    toJson() const = 0;

    virtual void
    run() = 0;

    virtual std::string
    toString() const = 0;

    virtual bool
    hasLedger(uint32_t sequence) const = 0;

    virtual std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(
        uint32_t ledgerSequence,
        bool getObjects = true,
        bool getObjectNeighbors = false) = 0;

    virtual bool
    loadInitialLedger(
        uint32_t sequence,
        std::uint32_t numMarkers,
        bool cacheOnly = false) = 0;

    virtual std::optional<boost::json::object>
    forwardToRippled(
        boost::json::object const& request,
        std::string const& clientIp,
        boost::asio::yield_context& yield) const = 0;

    virtual ~ETLSource()
    {
    }
};

template <class Derived>
class ETLSourceImpl : public ETLSource
{
    std::string wsPort_;

    std::string grpcPort_;

    std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub> stub_;

    boost::asio::ip::tcp::resolver resolver_;

    boost::beast::flat_buffer readBuffer_;

    std::vector<std::pair<uint32_t, uint32_t>> validatedLedgers_;

    std::string validatedLedgersRaw_;

    std::shared_ptr<NetworkValidatedLedgers> networkValidatedLedgers_;

    // beast::Journal journal_;

    mutable std::mutex mtx_;

    std::atomic_bool connected_{false};

    // true if this ETL source is forwarding transactions received on the
    // transactions_proposed stream. There are usually multiple ETL sources,
    // so to avoid forwarding the same transaction multiple times, we only
    // forward from one particular ETL source at a time.
    std::atomic_bool forwardingStream_{false};

    // The last time a message was received on the ledgers stream
    std::chrono::system_clock::time_point lastMsgTime_;
    mutable std::mutex lastMsgTimeMtx_;

    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    ETLLoadBalancer& balancer_;

protected:
    Derived&
    derived()
    {
        return static_cast<Derived&>(*this);
    }

    std::string ip_;

    size_t numFailures_ = 0;

    boost::asio::io_context& ioc_;

    // used for retrying connections
    boost::asio::steady_timer timer_;

    std::atomic_bool closing_{false};

    void
    run() override
    {
        BOOST_LOG_TRIVIAL(trace) << __func__ << " : " << toString();

        auto const host = ip_;
        auto const port = wsPort_;

        resolver_.async_resolve(host, port, [this](auto ec, auto results) {
            onResolve(ec, results);
        });
    }

public:
    ~ETLSourceImpl()
    {
        close(false);
    }

    bool
    isConnected() const override
    {
        return connected_;
    }

    std::chrono::system_clock::time_point
    getLastMsgTime() const
    {
        std::lock_guard lck(lastMsgTimeMtx_);
        return lastMsgTime_;
    }

    void
    setLastMsgTime()
    {
        std::lock_guard lck(lastMsgTimeMtx_);
        lastMsgTime_ = std::chrono::system_clock::now();
    }

    /// Create ETL source without gRPC endpoint
    /// Fetch ledger and load initial ledger will fail for this source
    /// Primarly used in read-only mode, to monitor when ledgers are validated
    ETLSourceImpl(
        boost::json::object const& config,
        boost::asio::io_context& ioContext,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> networkValidatedLedgers,
        ETLLoadBalancer& balancer);

    /// @param sequence ledger sequence to check for
    /// @return true if this source has the desired ledger
    bool
    hasLedger(uint32_t sequence) const override
    {
        std::lock_guard lck(mtx_);
        for (auto& pair : validatedLedgers_)
        {
            if (sequence >= pair.first && sequence <= pair.second)
            {
                return true;
            }
            else if (sequence < pair.first)
            {
                // validatedLedgers_ is a sorted list of disjoint ranges
                // if the sequence comes before this range, the sequence will
                // come before all subsequent ranges
                return false;
            }
        }
        return false;
    }

    /// process the validated range received on the ledgers stream. set the
    /// appropriate member variable
    /// @param range validated range received on ledgers stream
    void
    setValidatedRange(std::string const& range)
    {
        std::vector<std::pair<uint32_t, uint32_t>> pairs;
        std::vector<std::string> ranges;
        boost::split(ranges, range, boost::is_any_of(","));
        for (auto& pair : ranges)
        {
            std::vector<std::string> minAndMax;

            boost::split(minAndMax, pair, boost::is_any_of("-"));

            if (minAndMax.size() == 1)
            {
                uint32_t sequence = std::stoll(minAndMax[0]);
                pairs.push_back(std::make_pair(sequence, sequence));
            }
            else
            {
                assert(minAndMax.size() == 2);
                uint32_t min = std::stoll(minAndMax[0]);
                uint32_t max = std::stoll(minAndMax[1]);
                pairs.push_back(std::make_pair(min, max));
            }
        }
        std::sort(pairs.begin(), pairs.end(), [](auto left, auto right) {
            return left.first < right.first;
        });

        // we only hold the lock here, to avoid blocking while string processing
        std::lock_guard lck(mtx_);
        validatedLedgers_ = std::move(pairs);
        validatedLedgersRaw_ = range;
    }

    /// @return the validated range of this source
    /// @note this is only used by server_info
    std::string
    getValidatedRange() const
    {
        std::lock_guard lck(mtx_);

        return validatedLedgersRaw_;
    }

    /// Fetch the specified ledger
    /// @param ledgerSequence sequence of the ledger to fetch
    /// @getObjects whether to get the account state diff between this ledger
    /// and the prior one
    /// @return the extracted data and the result status
    std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(
        uint32_t ledgerSequence,
        bool getObjects = true,
        bool getObjectNeighbors = false) override;

    std::string
    toString() const override
    {
        return "{ validated_ledger : " + getValidatedRange() +
            " , ip : " + ip_ + " , web socket port : " + wsPort_ +
            ", grpc port : " + grpcPort_ + " }";
    }

    boost::json::object
    toJson() const override
    {
        boost::json::object res;
        res["validated_range"] = getValidatedRange();
        res["is_connected"] = std::to_string(isConnected());
        res["ip"] = ip_;
        res["ws_port"] = wsPort_;
        res["grpc_port"] = grpcPort_;
        auto last = getLastMsgTime();
        if (last.time_since_epoch().count() != 0)
            res["last_msg_arrival_time"] = std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now() - getLastMsgTime())
                    .count());
        return res;
    }

    /// Download a ledger in full
    /// @param ledgerSequence sequence of the ledger to download
    /// @param writeQueue queue to push downloaded ledger objects
    /// @return true if the download was successful
    bool
    loadInitialLedger(
        std::uint32_t ledgerSequence,
        std::uint32_t numMarkers,
        bool cacheOnly = false) override;

    /// Attempt to reconnect to the ETL source
    void
    reconnect(boost::beast::error_code ec);

    /// Callback
    void
    onResolve(
        boost::beast::error_code ec,
        boost::asio::ip::tcp::resolver::results_type results);

    /// Callback
    virtual void
    onConnect(
        boost::beast::error_code ec,
        boost::asio::ip::tcp::resolver::results_type::endpoint_type
            endpoint) = 0;

    /// Callback
    void
    onHandshake(boost::beast::error_code ec);

    /// Callback
    void
    onWrite(boost::beast::error_code ec, size_t size);

    /// Callback
    void
    onRead(boost::beast::error_code ec, size_t size);

    /// Handle the most recently received message
    /// @return true if the message was handled successfully. false on error
    bool
    handleMessage();

    std::optional<boost::json::object>
    forwardToRippled(
        boost::json::object const& request,
        std::string const& clientIp,
        boost::asio::yield_context& yield) const override;
};

class PlainETLSource : public ETLSourceImpl<PlainETLSource>
{
    std::unique_ptr<boost::beast::websocket::stream<boost::beast::tcp_stream>>
        ws_;

public:
    PlainETLSource(
        boost::json::object const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> nwvl,
        ETLLoadBalancer& balancer)
        : ETLSourceImpl(config, ioc, backend, subscriptions, nwvl, balancer)
        , ws_(std::make_unique<
              boost::beast::websocket::stream<boost::beast::tcp_stream>>(
              boost::asio::make_strand(ioc)))
    {
    }

    void
    onConnect(
        boost::beast::error_code ec,
        boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
        override;

    /// Close the websocket
    /// @param startAgain whether to reconnect
    void
    close(bool startAgain);

    boost::beast::websocket::stream<boost::beast::tcp_stream>&
    ws();
};

class SslETLSource : public ETLSourceImpl<SslETLSource>
{
    std::optional<std::reference_wrapper<boost::asio::ssl::context>> sslCtx_;

    std::unique_ptr<boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>>>
        ws_;

public:
    SslETLSource(
        boost::json::object const& config,
        boost::asio::io_context& ioc,
        std::optional<std::reference_wrapper<boost::asio::ssl::context>> sslCtx,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> nwvl,
        ETLLoadBalancer& balancer)
        : ETLSourceImpl(config, ioc, backend, subscriptions, nwvl, balancer)
        , sslCtx_(sslCtx)
        , ws_(std::make_unique<boost::beast::websocket::stream<
                  boost::beast::ssl_stream<boost::beast::tcp_stream>>>(
              boost::asio::make_strand(ioc_),
              *sslCtx_))
    {
    }

    void
    onConnect(
        boost::beast::error_code ec,
        boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
        override;

    void
    onSslHandshake(
        boost::beast::error_code ec,
        boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint);

    /// Close the websocket
    /// @param startAgain whether to reconnect
    void
    close(bool startAgain);

    boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>>&
    ws();
};

/// This class is used to manage connections to transaction processing processes
/// This class spawns a listener for each etl source, which listens to messages
/// on the ledgers stream (to keep track of which ledgers have been validated by
/// the network, and the range of ledgers each etl source has). This class also
/// allows requests for ledger data to be load balanced across all possible etl
/// sources.
class ETLLoadBalancer
{
private:
    std::vector<std::unique_ptr<ETLSource>> sources_;

    std::uint32_t downloadRanges_ = 16;

public:
    ETLLoadBalancer(
        boost::json::object const& config,
        boost::asio::io_context& ioContext,
        std::optional<std::reference_wrapper<boost::asio::ssl::context>> sslCtx,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> nwvl);

    static std::shared_ptr<ETLLoadBalancer>
    make_ETLLoadBalancer(
        boost::json::object const& config,
        boost::asio::io_context& ioc,
        std::optional<std::reference_wrapper<boost::asio::ssl::context>> sslCtx,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> validatedLedgers);

    ~ETLLoadBalancer();

    /// Load the initial ledger, writing data to the queue
    /// @param sequence sequence of ledger to download
    void
    loadInitialLedger(uint32_t sequence, bool cacheOnly = false);

    /// Fetch data for a specific ledger. This function will continuously try
    /// to fetch data for the specified ledger until the fetch succeeds, the
    /// ledger is found in the database, or the server is shutting down.
    /// @param ledgerSequence sequence of ledger to fetch data for
    /// @param getObjects if true, fetch diff between specified ledger and
    /// previous
    /// @return the extracted data, if extraction was successful. If the ledger
    /// was found in the database or the server is shutting down, the optional
    /// will be empty
    std::optional<org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(
        uint32_t ledgerSequence,
        bool getObjects,
        bool getObjectNeighbors);

    /// Determine whether messages received on the transactions_proposed stream
    /// should be forwarded to subscribing clients. The server subscribes to
    /// transactions_proposed on multiple ETLSources, yet only forwards messages
    /// from one source at any given time (to avoid sending duplicate messages
    /// to clients).
    /// @param in ETLSource in question
    /// @return true if messages should be forwarded
    bool
    shouldPropagateTxnStream(ETLSource* in) const;

    boost::json::value
    toJson() const;

    /// Forward a JSON RPC request to a randomly selected rippled node
    /// @param request JSON-RPC request
    /// @return response received from rippled node
    std::optional<boost::json::object>
    forwardToRippled(
        boost::json::object const& request,
        std::string const& clientIp,
        boost::asio::yield_context& yield) const;

private:
    /// f is a function that takes an ETLSource as an argument and returns a
    /// bool. Attempt to execute f for one randomly chosen ETLSource that has
    /// the specified ledger. If f returns false, another randomly chosen
    /// ETLSource is used. The process repeats until f returns true.
    /// @param f function to execute. This function takes the ETL source as an
    /// argument, and returns a bool.
    /// @param ledgerSequence f is executed for each ETLSource that has this
    /// ledger
    /// @return true if f was eventually executed successfully. false if the
    /// ledger was found in the database or the server is shutting down
    template <class Func>
    bool
    execute(Func f, uint32_t ledgerSequence);
};

#endif
