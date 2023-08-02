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
#include <etl/LoadBalancer.h>
#include <etl/impl/AsyncData.h>
#include <etl/impl/ForwardCache.h>
#include <log/Logger.h>
#include <subscriptions/SubscriptionManager.h>

#include <ripple/proto/org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <grpcpp/grpcpp.h>

class ProbingSource;
class SubscriptionManager;

// TODO: we use Source so that we can store a vector of Sources
// but we also use CRTP for implementation of the common logic - this is a bit strange because CRTP as used here is
// supposed to be used instead of an abstract base.
// Maybe we should rework this a bit. At this point there is not too much use in the CRTP implementation - we can move
// things into the base class instead.

/**
 * @brief Base class for all ETL sources
 */
class Source
{
public:
    virtual bool
    isConnected() const = 0;

    virtual boost::json::object
    toJson() const = 0;

    virtual void
    run() = 0;

    virtual void
    pause() = 0;

    virtual void
    resume() = 0;

    virtual std::string
    toString() const = 0;

    virtual bool
    hasLedger(uint32_t sequence) const = 0;

    virtual std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(uint32_t ledgerSequence, bool getObjects = true, bool getObjectNeighbors = false) = 0;

    virtual std::pair<std::vector<std::string>, bool>
    loadInitialLedger(uint32_t sequence, std::uint32_t numMarkers, bool cacheOnly = false) = 0;

    virtual std::optional<boost::json::object>
    forwardToRippled(boost::json::object const& request, std::string const& clientIp, boost::asio::yield_context yield)
        const = 0;

    virtual boost::uuids::uuid
    token() const = 0;

    virtual ~Source() = default;

    bool
    operator==(Source const& other) const
    {
        return token() == other.token();
    }

protected:
    clio::Logger log_{"ETL"};

private:
    friend clio::detail::ForwardCache;
    friend ProbingSource;

    virtual std::optional<boost::json::object>
    requestFromRippled(
        boost::json::object const& request,
        std::string const& clientIp,
        boost::asio::yield_context yield) const = 0;
};

/**
 * @brief Hooks for source events such as connects and disconnects
 */
struct SourceHooks
{
    enum class Action { STOP, PROCEED };

    std::function<Action(boost::beast::error_code)> onConnected;
    std::function<Action(boost::beast::error_code)> onDisconnected;
};

/**
 * @brief Base implementation of shared source logic (using CRTP)
 */
template <class Derived>
class SourceImpl : public Source
{
    std::string wsPort_;
    std::string grpcPort_;

    std::vector<std::pair<uint32_t, uint32_t>> validatedLedgers_;
    std::string validatedLedgersRaw_{"N/A"};
    std::shared_ptr<NetworkValidatedLedgers> networkValidatedLedgers_;

    mutable std::mutex mtx_;
    std::atomic_bool connected_{false};

    // true if this ETL source is forwarding transactions received on the transactions_proposed stream. There are
    // usually multiple ETL sources, so to avoid forwarding the same transaction multiple times, we only forward from
    // one particular ETL source at a time.
    std::atomic_bool forwardingStream_{false};

    std::chrono::system_clock::time_point lastMsgTime_;
    mutable std::mutex lastMsgTimeMtx_;

    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<SubscriptionManager> subscriptions_;
    LoadBalancer& balancer_;

    clio::detail::ForwardCache forwardCache_;
    boost::uuids::uuid uuid_;

protected:
    std::string ip_;
    size_t numFailures_ = 0;

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    boost::asio::steady_timer timer_;
    boost::asio::ip::tcp::resolver resolver_;
    boost::beast::flat_buffer readBuffer_;

    std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub> stub_;

    std::atomic_bool closing_{false};
    std::atomic_bool paused_{false};

    SourceHooks hooks_;

public:
    /**
     * @brief Create ETL source without gRPC endpoint
     *
     * Fetch ledger and load initial ledger will fail for this source.
     * Primarly used in read-only mode, to monitor when ledgers are validated.
     */
    SourceImpl(
        clio::Config const& config,
        boost::asio::io_context& ioContext,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> networkValidatedLedgers,
        LoadBalancer& balancer,
        SourceHooks hooks)
        : networkValidatedLedgers_(networkValidatedLedgers)
        , backend_(backend)
        , subscriptions_(subscriptions)
        , balancer_(balancer)
        , forwardCache_(config, ioContext, *this)
        , strand_(boost::asio::make_strand(ioContext))
        , timer_(strand_)
        , resolver_(strand_)
        , hooks_(hooks)
    {
        static boost::uuids::random_generator uuidGenerator;
        uuid_ = uuidGenerator();

        ip_ = config.valueOr<std::string>("ip", {});
        wsPort_ = config.valueOr<std::string>("ws_port", {});

        if (auto value = config.maybeValue<std::string>("grpc_port"); value)
        {
            grpcPort_ = *value;
            try
            {
                boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::make_address(ip_), std::stoi(grpcPort_)};
                std::stringstream ss;
                ss << endpoint;
                grpc::ChannelArguments chArgs;
                chArgs.SetMaxReceiveMessageSize(-1);
                stub_ = org::xrpl::rpc::v1::XRPLedgerAPIService::NewStub(
                    grpc::CreateCustomChannel(ss.str(), grpc::InsecureChannelCredentials(), chArgs));
                log_.debug() << "Made stub for remote = " << toString();
            }
            catch (std::exception const& e)
            {
                log_.debug() << "Exception while creating stub = " << e.what() << " . Remote = " << toString();
            }
        }
    }

    ~SourceImpl()
    {
        derived().close(false);
    }

    bool
    isConnected() const override
    {
        return connected_;
    }

    boost::uuids::uuid
    token() const override
    {
        return uuid_;
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

    std::optional<boost::json::object>
    requestFromRippled(
        boost::json::object const& request,
        std::string const& clientIp,
        boost::asio::yield_context yield) const override
    {
        log_.trace() << "Attempting to forward request to tx. "
                     << "request = " << boost::json::serialize(request);

        boost::json::object response;
        if (!connected_)
        {
            log_.error() << "Attempted to proxy but failed to connect to tx";
            return {};
        }

        namespace beast = boost::beast;
        namespace http = beast::http;
        namespace websocket = beast::websocket;
        namespace net = boost::asio;
        using tcp = boost::asio::ip::tcp;

        try
        {
            auto executor = boost::asio::get_associated_executor(yield);
            boost::beast::error_code ec;
            tcp::resolver resolver{executor};

            auto ws = std::make_unique<websocket::stream<beast::tcp_stream>>(executor);

            auto const results = resolver.async_resolve(ip_, wsPort_, yield[ec]);
            if (ec)
                return {};

            ws->next_layer().expires_after(std::chrono::seconds(3));
            ws->next_layer().async_connect(results, yield[ec]);
            if (ec)
                return {};

            // Set a decorator to change the User-Agent of the handshake and to tell rippled to charge the client IP for
            // RPC resources. See "secure_gateway" in
            // https://github.com/ripple/rippled/blob/develop/cfg/rippled-example.cfg
            ws->set_option(websocket::stream_base::decorator([&clientIp](websocket::request_type& req) {
                req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
                req.set(http::field::forwarded, "for=" + clientIp);
            }));

            ws->async_handshake(ip_, "/", yield[ec]);
            if (ec)
                return {};

            ws->async_write(net::buffer(boost::json::serialize(request)), yield[ec]);
            if (ec)
                return {};

            beast::flat_buffer buffer;
            ws->async_read(buffer, yield[ec]);
            if (ec)
                return {};

            auto begin = static_cast<char const*>(buffer.data().data());
            auto end = begin + buffer.data().size();
            auto parsed = boost::json::parse(std::string(begin, end));

            if (!parsed.is_object())
            {
                log_.error() << "Error parsing response: " << std::string{begin, end};
                return {};
            }

            response = parsed.as_object();
            response["forwarded"] = true;

            return response;
        }
        catch (std::exception const& e)
        {
            log_.error() << "Encountered exception : " << e.what();
            return {};
        }
    }

    /**
     * @param sequence ledger sequence to check for
     * @return true if this source has the desired ledger
     */
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

    /**
     * @brief Process the validated range received on the ledgers stream. set the appropriate member variable
     *
     * @param range validated range received on ledgers stream
     */
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
        std::sort(pairs.begin(), pairs.end(), [](auto left, auto right) { return left.first < right.first; });

        // we only hold the lock here, to avoid blocking while string processing
        std::lock_guard lck(mtx_);
        validatedLedgers_ = std::move(pairs);
        validatedLedgersRaw_ = range;
    }

    /**
     * @return the validated range of this source
     * @note this is only used by server_info
     */
    std::string
    getValidatedRange() const
    {
        std::lock_guard lck(mtx_);
        return validatedLedgersRaw_;
    }

    /**
     * @brief Fetch the specified ledger
     *
     * @param ledgerSequence sequence of the ledger to fetch @getObjects whether to get the account state diff between
     * this ledger and the prior one
     * @return the extracted data and the result status
     */
    std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
    fetchLedger(uint32_t ledgerSequence, bool getObjects = true, bool getObjectNeighbors = false) override
    {
        org::xrpl::rpc::v1::GetLedgerResponse response;
        if (!stub_)
            return {{grpc::StatusCode::INTERNAL, "No Stub"}, response};

        // Ledger header with txns and metadata
        org::xrpl::rpc::v1::GetLedgerRequest request;
        grpc::ClientContext context;

        request.mutable_ledger()->set_sequence(ledgerSequence);
        request.set_transactions(true);
        request.set_expand(true);
        request.set_get_objects(getObjects);
        request.set_get_object_neighbors(getObjectNeighbors);
        request.set_user("ETL");

        grpc::Status status = stub_->GetLedger(&context, request, &response);

        if (status.ok() && !response.is_unlimited())
        {
            log_.warn()
                << "is_unlimited is false. Make sure secure_gateway is set correctly on the ETL source. source = "
                << toString() << "; status = " << status.error_message();
        }

        return {status, std::move(response)};
    }

    /**
     * @brief Produces a human-readable string with info about the source
     */
    std::string
    toString() const override
    {
        return "{validated_ledger: " + getValidatedRange() + ", ip: " + ip_ + ", web socket port: " + wsPort_ +
            ", grpc port: " + grpcPort_ + "}";
    }

    /**
     * @brief Produces stats for this source in a json object
     * @return json object with stats
     */
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
            res["last_msg_age_seconds"] = std::to_string(
                std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - getLastMsgTime())
                    .count());

        return res;
    }

    /**
     * @brief Download a ledger in full
     *
     * @param ledgerSequence sequence of the ledger to download
     * @param writeQueue queue to push downloaded ledger objects
     * @return true if the download was successful
     */
    std::pair<std::vector<std::string>, bool>
    loadInitialLedger(std::uint32_t ledgerSequence, std::uint32_t numMarkers, bool cacheOnly = false) override
    {
        if (!stub_)
            return {{}, false};

        grpc::CompletionQueue cq;
        void* tag;
        bool ok = false;
        std::vector<clio::detail::AsyncCallData> calls;
        auto markers = getMarkers(numMarkers);

        for (size_t i = 0; i < markers.size(); ++i)
        {
            std::optional<ripple::uint256> nextMarker;

            if (i + 1 < markers.size())
                nextMarker = markers[i + 1];

            calls.emplace_back(ledgerSequence, markers[i], nextMarker);
        }

        log_.debug() << "Starting data download for ledger " << ledgerSequence << ". Using source = " << toString();

        for (auto& c : calls)
            c.call(stub_, cq);

        size_t numFinished = 0;
        bool abort = false;
        size_t incr = 500000;
        size_t progress = incr;
        std::vector<std::string> edgeKeys;

        while (numFinished < calls.size() && cq.Next(&tag, &ok))
        {
            assert(tag);
            auto ptr = static_cast<clio::detail::AsyncCallData*>(tag);

            if (!ok)
            {
                log_.error() << "loadInitialLedger - ok is false";
                return {{}, false};  // handle cancelled
            }
            else
            {
                log_.trace() << "Marker prefix = " << ptr->getMarkerPrefix();

                auto result = ptr->process(stub_, cq, *backend_, abort, cacheOnly);
                if (result != clio::detail::AsyncCallData::CallStatus::MORE)
                {
                    ++numFinished;
                    log_.debug() << "Finished a marker. "
                                 << "Current number of finished = " << numFinished;

                    std::string lastKey = ptr->getLastKey();

                    if (lastKey.size())
                        edgeKeys.push_back(ptr->getLastKey());
                }

                if (result == clio::detail::AsyncCallData::CallStatus::ERRORED)
                    abort = true;

                if (backend_->cache().size() > progress)
                {
                    log_.info() << "Downloaded " << backend_->cache().size() << " records from rippled";
                    progress += incr;
                }
            }
        }

        log_.info() << "Finished loadInitialLedger. cache size = " << backend_->cache().size();
        return {std::move(edgeKeys), !abort};
    }

    /**
     * @brief Attempt to reconnect to the ETL source
     */
    void
    reconnect(boost::beast::error_code ec)
    {
        if (paused_)
            return;

        if (connected_)
            hooks_.onDisconnected(ec);

        connected_ = false;
        readBuffer_ = {};

        // These are somewhat normal errors. operation_aborted occurs on shutdown,
        // when the timer is cancelled. connection_refused will occur repeatedly
        std::string err = ec.message();
        // if we cannot connect to the transaction processing process
        if (ec.category() == boost::asio::error::get_ssl_category())
        {
            err = std::string(" (") + boost::lexical_cast<std::string>(ERR_GET_LIB(ec.value())) + "," +
                boost::lexical_cast<std::string>(ERR_GET_REASON(ec.value())) + ") ";

            // ERR_PACK /* crypto/err/err.h */
            char buf[128];
            ::ERR_error_string_n(ec.value(), buf, sizeof(buf));
            err += buf;

            log_.error() << err;
        }

        if (ec != boost::asio::error::operation_aborted && ec != boost::asio::error::connection_refused)
        {
            log_.error() << "error code = " << ec << " - " << toString();
        }
        else
        {
            log_.warn() << "error code = " << ec << " - " << toString();
        }

        // exponentially increasing timeouts, with a max of 30 seconds
        size_t waitTime = std::min(pow(2, numFailures_), 30.0);
        numFailures_++;
        timer_.expires_after(boost::asio::chrono::seconds(waitTime));
        timer_.async_wait([this](auto ec) {
            bool startAgain = (ec != boost::asio::error::operation_aborted);
            derived().close(startAgain);
        });
    }

    /**
     * @brief Pause the source effectively stopping it from trying to reconnect
     */
    void
    pause() override
    {
        paused_ = true;
        derived().close(false);
    }

    /**
     * @brief Resume the source allowing it to reconnect again
     */
    void
    resume() override
    {
        paused_ = false;
        derived().close(true);
    }

    /**
     * @brief Callback for resolving the server host
     */
    void
    onResolve(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results)
    {
        if (ec)
        {
            // try again
            reconnect(ec);
        }
        else
        {
            boost::beast::get_lowest_layer(derived().ws()).expires_after(std::chrono::seconds(30));
            boost::beast::get_lowest_layer(derived().ws()).async_connect(results, [this](auto ec, auto ep) {
                derived().onConnect(ec, ep);
            });
        }
    }

    /**
     * @brief Callback for handshake with the server
     */
    void
    onHandshake(boost::beast::error_code ec)
    {
        if (auto action = hooks_.onConnected(ec); action == SourceHooks::Action::STOP)
            return;

        if (ec)
        {
            // start over
            reconnect(ec);
        }
        else
        {
            boost::json::object jv{
                {"command", "subscribe"},
                {"streams", {"ledger", "manifests", "validations", "transactions_proposed"}},
            };
            std::string s = boost::json::serialize(jv);
            log_.trace() << "Sending subscribe stream message";

            derived().ws().set_option(
                boost::beast::websocket::stream_base::decorator([](boost::beast::websocket::request_type& req) {
                    req.set(
                        boost::beast::http::field::user_agent,
                        std::string(BOOST_BEAST_VERSION_STRING) + " clio-client");
                    req.set("X-User", "coro-client");
                }));

            // Send subscription message
            derived().ws().async_write(boost::asio::buffer(s), [this](auto ec, size_t size) { onWrite(ec, size); });
        }
    }

    /**
     * @brief Callback for writing data
     */
    void
    onWrite(boost::beast::error_code ec, size_t size)
    {
        if (ec)
            reconnect(ec);
        else
            derived().ws().async_read(readBuffer_, [this](auto ec, size_t size) { onRead(ec, size); });
    }

    /**
     * @brief Callback for data available to read
     */
    void
    onRead(boost::beast::error_code ec, size_t size)
    {
        if (ec)
        {
            reconnect(ec);
        }
        else
        {
            handleMessage(size);
            derived().ws().async_read(readBuffer_, [this](auto ec, size_t size) { onRead(ec, size); });
        }
    }

    /**
     * @brief Handle the most recently received message
     * @return true if the message was handled successfully. false on error
     */
    bool
    handleMessage(size_t size)
    {
        setLastMsgTime();
        connected_ = true;

        try
        {
            auto const msg = boost::beast::buffers_to_string(readBuffer_.data());
            readBuffer_.consume(size);

            auto const raw = boost::json::parse(msg);
            auto const response = raw.as_object();
            uint32_t ledgerIndex = 0;

            if (response.contains("result"))
            {
                auto const& result = response.at("result").as_object();
                if (result.contains("ledger_index"))
                    ledgerIndex = result.at("ledger_index").as_int64();

                if (result.contains("validated_ledgers"))
                {
                    auto const& validatedLedgers = result.at("validated_ledgers").as_string();
                    setValidatedRange({validatedLedgers.data(), validatedLedgers.size()});
                }

                log_.info() << "Received a message on ledger "
                            << " subscription stream. Message : " << response << " - " << toString();
            }
            else if (response.contains("type") && response.at("type") == "ledgerClosed")
            {
                log_.info() << "Received a message on ledger "
                            << " subscription stream. Message : " << response << " - " << toString();
                if (response.contains("ledger_index"))
                {
                    ledgerIndex = response.at("ledger_index").as_int64();
                }
                if (response.contains("validated_ledgers"))
                {
                    auto const& validatedLedgers = response.at("validated_ledgers").as_string();
                    setValidatedRange({validatedLedgers.data(), validatedLedgers.size()});
                }
            }
            else
            {
                if (balancer_.shouldPropagateTxnStream(this))
                {
                    if (response.contains("transaction"))
                    {
                        forwardCache_.freshen();
                        subscriptions_->forwardProposedTransaction(response);
                    }
                    else if (response.contains("type") && response.at("type") == "validationReceived")
                    {
                        subscriptions_->forwardValidation(response);
                    }
                    else if (response.contains("type") && response.at("type") == "manifestReceived")
                    {
                        subscriptions_->forwardManifest(response);
                    }
                }
            }

            if (ledgerIndex != 0)
            {
                log_.trace() << "Pushing ledger sequence = " << ledgerIndex << " - " << toString();
                networkValidatedLedgers_->push(ledgerIndex);
            }

            return true;
        }
        catch (std::exception const& e)
        {
            log_.error() << "Exception in handleMessage : " << e.what();
            return false;
        }
    }

    /**
     * @brief Forward a request to rippled
     * @return response wrapped in an optional on success; nullopt otherwise
     */
    std::optional<boost::json::object>
    forwardToRippled(boost::json::object const& request, std::string const& clientIp, boost::asio::yield_context yield)
        const override
    {
        if (auto resp = forwardCache_.get(request); resp)
        {
            log_.debug() << "request hit forwardCache";
            return resp;
        }

        return requestFromRippled(request, clientIp, yield);
    }

protected:
    Derived&
    derived()
    {
        return static_cast<Derived&>(*this);
    }

    void
    run() override
    {
        resolver_.async_resolve(ip_, wsPort_, [this](auto ec, auto results) { onResolve(ec, results); });
    }
};

class PlainSource : public SourceImpl<PlainSource>
{
    using StreamType = boost::beast::websocket::stream<boost::beast::tcp_stream>;
    std::unique_ptr<StreamType> ws_;

public:
    PlainSource(
        clio::Config const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> nwvl,
        LoadBalancer& balancer,
        SourceHooks hooks)
        : SourceImpl(config, ioc, backend, subscriptions, nwvl, balancer, std::move(hooks))
        , ws_(std::make_unique<StreamType>(strand_))
    {
    }

    /**
     * @brief Callback for connection to the server
     */
    void
    onConnect(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint);

    /**
     * @brief Close the websocket
     * @param startAgain whether to reconnect
     */
    void
    close(bool startAgain);

    boost::beast::websocket::stream<boost::beast::tcp_stream>&
    ws()
    {
        return *ws_;
    }
};

class SslSource : public SourceImpl<SslSource>
{
    using StreamType = boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>;
    std::optional<std::reference_wrapper<boost::asio::ssl::context>> sslCtx_;
    std::unique_ptr<StreamType> ws_;

public:
    SslSource(
        clio::Config const& config,
        boost::asio::io_context& ioc,
        std::optional<std::reference_wrapper<boost::asio::ssl::context>> sslCtx,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManager> subscriptions,
        std::shared_ptr<NetworkValidatedLedgers> nwvl,
        LoadBalancer& balancer,
        SourceHooks hooks)
        : SourceImpl(config, ioc, backend, subscriptions, nwvl, balancer, std::move(hooks))
        , sslCtx_(sslCtx)
        , ws_(std::make_unique<StreamType>(strand_, *sslCtx_))
    {
    }

    /**
     * @brief Callback for connection to the server
     */
    void
    onConnect(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint);

    /**
     * @brief Callback for SSL handshake completion
     */
    void
    onSslHandshake(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint);

    /**
     * @brief Close the websocket
     * @param startAgain whether to reconnect
     */
    void
    close(bool startAgain);

    StreamType&
    ws()
    {
        return *ws_;
    }
};
