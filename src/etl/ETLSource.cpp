#include <ripple/beast/net/IPEndpoint.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <boost/asio/strand.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <boost/json/src.hpp>

#include <backend/DBHelpers.h>
#include <etl/ETLSource.h>
#include <etl/ProbingETLSource.h>
#include <etl/ReportingETL.h>
#include <log/Logger.h>
#include <rpc/RPCHelpers.h>

#include <thread>

using namespace clio;

void
ForwardCache::freshen()
{
    log_.trace() << "Freshening ForwardCache";

    auto numOutstanding =
        std::make_shared<std::atomic_uint>(latestForwarded_.size());

    for (auto const& cacheEntry : latestForwarded_)
    {
        boost::asio::spawn(
            strand_,
            [this, numOutstanding, command = cacheEntry.first](
                boost::asio::yield_context yield) {
                boost::json::object request = {{"command", command}};
                auto resp = source_.requestFromRippled(request, {}, yield);

                if (!resp || resp->contains("error"))
                    resp = {};

                {
                    std::unique_lock lk(mtx_);
                    latestForwarded_[command] = resp;
                }
            });
    }
}

void
ForwardCache::clear()
{
    std::unique_lock lk(mtx_);
    for (auto& cacheEntry : latestForwarded_)
        latestForwarded_[cacheEntry.first] = {};
}

std::optional<boost::json::object>
ForwardCache::get(boost::json::object const& request) const
{
    std::optional<std::string> command = {};
    if (request.contains("command") && !request.contains("method") &&
        request.at("command").is_string())
        command = request.at("command").as_string().c_str();
    else if (
        request.contains("method") && !request.contains("command") &&
        request.at("method").is_string())
        command = request.at("method").as_string().c_str();

    if (!command)
        return {};
    if (RPC::specifiesCurrentOrClosedLedger(request))
        return {};

    std::shared_lock lk(mtx_);
    if (!latestForwarded_.contains(*command))
        return {};

    return {latestForwarded_.at(*command)};
}

static boost::beast::websocket::stream_base::timeout
make_TimeoutOption()
{
    // See #289 for details.
    // TODO: investigate the issue and find if there is a solution other than
    // introducing artificial timeouts.
    if (true)
    {
        // The only difference between this and the suggested client role is
        // that idle_timeout is set to 20 instead of none()
        auto opt = boost::beast::websocket::stream_base::timeout{};
        opt.handshake_timeout = std::chrono::seconds(30);
        opt.idle_timeout = std::chrono::seconds(20);
        opt.keep_alive_pings = false;
        return opt;
    }
    else
    {
        return boost::beast::websocket::stream_base::timeout::suggested(
            boost::beast::role_type::client);
    }
}

template <class Derived>
void
ETLSourceImpl<Derived>::reconnect(boost::beast::error_code ec)
{
    if (paused_)
        return;

    if (connected_)
        hooks_.onDisconnected(ec);

    connected_ = false;
    // These are somewhat normal errors. operation_aborted occurs on shutdown,
    // when the timer is cancelled. connection_refused will occur repeatedly
    std::string err = ec.message();
    // if we cannot connect to the transaction processing process
    if (ec.category() == boost::asio::error::get_ssl_category())
    {
        err = std::string(" (") +
            boost::lexical_cast<std::string>(ERR_GET_LIB(ec.value())) + "," +
            boost::lexical_cast<std::string>(ERR_GET_REASON(ec.value())) + ") ";
        // ERR_PACK /* crypto/err/err.h */
        char buf[128];
        ::ERR_error_string_n(ec.value(), buf, sizeof(buf));
        err += buf;

        std::cout << err << std::endl;
    }

    if (ec != boost::asio::error::operation_aborted &&
        ec != boost::asio::error::connection_refused)
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
        log_.trace() << "async_wait : ec = " << ec;
        derived().close(startAgain);
    });
}

void
PlainETLSource::close(bool startAgain)
{
    timer_.cancel();
    ioc_.post([this, startAgain]() {
        if (closing_)
            return;

        if (derived().ws().is_open())
        {
            // onStop() also calls close(). If the async_close is called twice,
            // an assertion fails. Using closing_ makes sure async_close is only
            // called once
            closing_ = true;
            derived().ws().async_close(
                boost::beast::websocket::close_code::normal,
                [this, startAgain](auto ec) {
                    if (ec)
                    {
                        log_.error()
                            << " async_close : "
                            << "error code = " << ec << " - " << toString();
                    }
                    closing_ = false;
                    if (startAgain)
                    {
                        ws_ = std::make_unique<boost::beast::websocket::stream<
                            boost::beast::tcp_stream>>(
                            boost::asio::make_strand(ioc_));

                        run();
                    }
                });
        }
        else if (startAgain)
        {
            ws_ = std::make_unique<
                boost::beast::websocket::stream<boost::beast::tcp_stream>>(
                boost::asio::make_strand(ioc_));

            run();
        }
    });
}

void
SslETLSource::close(bool startAgain)
{
    timer_.cancel();
    ioc_.post([this, startAgain]() {
        if (closing_)
            return;

        if (derived().ws().is_open())
        {
            // onStop() also calls close(). If the async_close is called twice,
            // an assertion fails. Using closing_ makes sure async_close is only
            // called once
            closing_ = true;
            derived().ws().async_close(
                boost::beast::websocket::close_code::normal,
                [this, startAgain](auto ec) {
                    if (ec)
                    {
                        log_.error()
                            << " async_close : "
                            << "error code = " << ec << " - " << toString();
                    }
                    closing_ = false;
                    if (startAgain)
                    {
                        ws_ = std::make_unique<boost::beast::websocket::stream<
                            boost::beast::ssl_stream<
                                boost::beast::tcp_stream>>>(
                            boost::asio::make_strand(ioc_), *sslCtx_);

                        run();
                    }
                });
        }
        else if (startAgain)
        {
            ws_ = std::make_unique<boost::beast::websocket::stream<
                boost::beast::ssl_stream<boost::beast::tcp_stream>>>(
                boost::asio::make_strand(ioc_), *sslCtx_);

            run();
        }
    });
}

template <class Derived>
void
ETLSourceImpl<Derived>::onResolve(
    boost::beast::error_code ec,
    boost::asio::ip::tcp::resolver::results_type results)
{
    log_.trace() << "ec = " << ec << " - " << toString();
    if (ec)
    {
        // try again
        reconnect(ec);
    }
    else
    {
        boost::beast::get_lowest_layer(derived().ws())
            .expires_after(std::chrono::seconds(30));
        boost::beast::get_lowest_layer(derived().ws())
            .async_connect(results, [this](auto ec, auto ep) {
                derived().onConnect(ec, ep);
            });
    }
}

void
PlainETLSource::onConnect(
    boost::beast::error_code ec,
    boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    log_.trace() << "ec = " << ec << " - " << toString();
    if (ec)
    {
        // start over
        reconnect(ec);
    }
    else
    {
        numFailures_ = 0;
        // Turn off timeout on the tcp stream, because websocket stream has it's
        // own timeout system
        boost::beast::get_lowest_layer(derived().ws()).expires_never();

        // Set a desired timeout for the websocket stream
        derived().ws().set_option(make_TimeoutOption());

        // Set a decorator to change the User-Agent of the handshake
        derived().ws().set_option(
            boost::beast::websocket::stream_base::decorator(
                [](boost::beast::websocket::request_type& req) {
                    req.set(
                        boost::beast::http::field::user_agent, "clio-client");

                    req.set("X-User", "clio-client");
                }));

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        auto host = ip_ + ':' + std::to_string(endpoint.port());
        // Perform the websocket handshake
        derived().ws().async_handshake(
            host, "/", [this](auto ec) { onHandshake(ec); });
    }
}

void
SslETLSource::onConnect(
    boost::beast::error_code ec,
    boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    log_.trace() << "ec = " << ec << " - " << toString();
    if (ec)
    {
        // start over
        reconnect(ec);
    }
    else
    {
        numFailures_ = 0;
        // Turn off timeout on the tcp stream, because websocket stream has it's
        // own timeout system
        boost::beast::get_lowest_layer(derived().ws()).expires_never();

        // Set a desired timeout for the websocket stream
        derived().ws().set_option(make_TimeoutOption());

        // Set a decorator to change the User-Agent of the handshake
        derived().ws().set_option(
            boost::beast::websocket::stream_base::decorator(
                [](boost::beast::websocket::request_type& req) {
                    req.set(
                        boost::beast::http::field::user_agent, "clio-client");

                    req.set("X-User", "clio-client");
                }));

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        auto host = ip_ + ':' + std::to_string(endpoint.port());
        // Perform the websocket handshake
        ws().next_layer().async_handshake(
            boost::asio::ssl::stream_base::client,
            [this, endpoint](auto ec) { onSslHandshake(ec, endpoint); });
    }
}

void
SslETLSource::onSslHandshake(
    boost::beast::error_code ec,
    boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    if (ec)
    {
        reconnect(ec);
    }
    else
    {
        // Perform the websocket handshake
        auto host = ip_ + ':' + std::to_string(endpoint.port());
        // Perform the websocket handshake
        ws().async_handshake(host, "/", [this](auto ec) { onHandshake(ec); });
    }
}

template <class Derived>
void
ETLSourceImpl<Derived>::onHandshake(boost::beast::error_code ec)
{
    log_.trace() << "ec = " << ec << " - " << toString();
    if (auto action = hooks_.onConnected(ec);
        action == ETLSourceHooks::Action::STOP)
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
            {"streams",
             {"ledger", "manifests", "validations", "transactions_proposed"}}};
        std::string s = boost::json::serialize(jv);
        log_.trace() << "Sending subscribe stream message";

        derived().ws().set_option(
            boost::beast::websocket::stream_base::decorator(
                [](boost::beast::websocket::request_type& req) {
                    req.set(
                        boost::beast::http::field::user_agent,
                        std::string(BOOST_BEAST_VERSION_STRING) +
                            " clio-client");

                    req.set("X-User", "coro-client");
                }));

        // Send the message
        derived().ws().async_write(
            boost::asio::buffer(s),
            [this](auto ec, size_t size) { onWrite(ec, size); });
    }
}

template <class Derived>
void
ETLSourceImpl<Derived>::onWrite(
    boost::beast::error_code ec,
    size_t bytesWritten)
{
    log_.trace() << "ec = " << ec << " - " << toString();
    if (ec)
    {
        // start over
        reconnect(ec);
    }
    else
    {
        derived().ws().async_read(
            readBuffer_, [this](auto ec, size_t size) { onRead(ec, size); });
    }
}

template <class Derived>
void
ETLSourceImpl<Derived>::onRead(boost::beast::error_code ec, size_t size)
{
    log_.trace() << "ec = " << ec << " - " << toString();
    // if error or error reading message, start over
    if (ec)
    {
        reconnect(ec);
    }
    else
    {
        handleMessage();
        boost::beast::flat_buffer buffer;
        swap(readBuffer_, buffer);

        log_.trace() << "calling async_read - " << toString();
        derived().ws().async_read(
            readBuffer_, [this](auto ec, size_t size) { onRead(ec, size); });
    }
}

template <class Derived>
bool
ETLSourceImpl<Derived>::handleMessage()
{
    log_.trace() << toString();

    setLastMsgTime();
    connected_ = true;
    try
    {
        std::string msg{
            static_cast<char const*>(readBuffer_.data().data()),
            readBuffer_.size()};
        log_.trace() << msg;
        boost::json::value raw = boost::json::parse(msg);
        log_.trace() << "parsed";
        boost::json::object response = raw.as_object();

        uint32_t ledgerIndex = 0;
        if (response.contains("result"))
        {
            boost::json::object result = response["result"].as_object();
            if (result.contains("ledger_index"))
            {
                ledgerIndex = result["ledger_index"].as_int64();
            }
            if (result.contains("validated_ledgers"))
            {
                boost::json::string const& validatedLedgers =
                    result["validated_ledgers"].as_string();

                setValidatedRange(
                    {validatedLedgers.c_str(), validatedLedgers.size()});
            }
            log_.info() << "Received a message on ledger "
                        << " subscription stream. Message : " << response
                        << " - " << toString();
        }
        else if (
            response.contains("type") && response["type"] == "ledgerClosed")
        {
            log_.info() << "Received a message on ledger "
                        << " subscription stream. Message : " << response
                        << " - " << toString();
            if (response.contains("ledger_index"))
            {
                ledgerIndex = response["ledger_index"].as_int64();
            }
            if (response.contains("validated_ledgers"))
            {
                boost::json::string const& validatedLedgers =
                    response["validated_ledgers"].as_string();
                setValidatedRange(
                    {validatedLedgers.c_str(), validatedLedgers.size()});
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
                else if (
                    response.contains("type") &&
                    response["type"] == "validationReceived")
                {
                    subscriptions_->forwardValidation(response);
                }
                else if (
                    response.contains("type") &&
                    response["type"] == "manifestReceived")
                {
                    subscriptions_->forwardManifest(response);
                }
            }
        }

        if (ledgerIndex != 0)
        {
            log_.trace() << "Pushing ledger sequence = " << ledgerIndex << " - "
                         << toString();
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

class AsyncCallData
{
    clio::Logger log_{"ETL"};

    std::unique_ptr<org::xrpl::rpc::v1::GetLedgerDataResponse> cur_;
    std::unique_ptr<org::xrpl::rpc::v1::GetLedgerDataResponse> next_;

    org::xrpl::rpc::v1::GetLedgerDataRequest request_;
    std::unique_ptr<grpc::ClientContext> context_;

    grpc::Status status_;
    unsigned char nextPrefix_;

    std::string lastKey_;

public:
    AsyncCallData(
        uint32_t seq,
        ripple::uint256 const& marker,
        std::optional<ripple::uint256> const& nextMarker)
    {
        request_.mutable_ledger()->set_sequence(seq);
        if (marker.isNonZero())
        {
            request_.set_marker(marker.data(), marker.size());
        }
        request_.set_user("ETL");
        nextPrefix_ = 0x00;
        if (nextMarker)
            nextPrefix_ = nextMarker->data()[0];

        unsigned char prefix = marker.data()[0];

        log_.debug() << "Setting up AsyncCallData. marker = "
                     << ripple::strHex(marker)
                     << " . prefix = " << ripple::strHex(std::string(1, prefix))
                     << " . nextPrefix_ = "
                     << ripple::strHex(std::string(1, nextPrefix_));

        assert(nextPrefix_ > prefix || nextPrefix_ == 0x00);

        cur_ = std::make_unique<org::xrpl::rpc::v1::GetLedgerDataResponse>();

        next_ = std::make_unique<org::xrpl::rpc::v1::GetLedgerDataResponse>();

        context_ = std::make_unique<grpc::ClientContext>();
    }

    enum class CallStatus { MORE, DONE, ERRORED };
    CallStatus
    process(
        std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub>& stub,
        grpc::CompletionQueue& cq,
        BackendInterface& backend,
        bool abort,
        bool cacheOnly = false)
    {
        log_.trace() << "Processing response. "
                     << "Marker prefix = " << getMarkerPrefix();
        if (abort)
        {
            log_.error() << "AsyncCallData aborted";
            return CallStatus::ERRORED;
        }
        if (!status_.ok())
        {
            log_.error() << "AsyncCallData status_ not ok: "
                         << " code = " << status_.error_code()
                         << " message = " << status_.error_message();
            return CallStatus::ERRORED;
        }
        if (!next_->is_unlimited())
        {
            log_.warn() << "AsyncCallData is_unlimited is false. Make sure "
                           "secure_gateway is set correctly at the ETL source";
        }

        std::swap(cur_, next_);

        bool more = true;

        // if no marker returned, we are done
        if (cur_->marker().size() == 0)
            more = false;

        // if returned marker is greater than our end, we are done
        unsigned char prefix = cur_->marker()[0];
        if (nextPrefix_ != 0x00 && prefix >= nextPrefix_)
            more = false;

        // if we are not done, make the next async call
        if (more)
        {
            request_.set_marker(std::move(cur_->marker()));
            call(stub, cq);
        }

        log_.trace() << "Writing objects";
        std::vector<Backend::LedgerObject> cacheUpdates;
        cacheUpdates.reserve(cur_->ledger_objects().objects_size());
        for (int i = 0; i < cur_->ledger_objects().objects_size(); ++i)
        {
            auto& obj = *(cur_->mutable_ledger_objects()->mutable_objects(i));
            if (!more && nextPrefix_ != 0x00)
            {
                if (((unsigned char)obj.key()[0]) >= nextPrefix_)
                    continue;
            }
            cacheUpdates.push_back(
                {*ripple::uint256::fromVoidChecked(obj.key()),
                 {obj.mutable_data()->begin(), obj.mutable_data()->end()}});
            if (!cacheOnly)
            {
                if (lastKey_.size())
                    backend.writeSuccessor(
                        std::move(lastKey_),
                        request_.ledger().sequence(),
                        std::string{obj.key()});
                lastKey_ = obj.key();
                backend.writeLedgerObject(
                    std::move(*obj.mutable_key()),
                    request_.ledger().sequence(),
                    std::move(*obj.mutable_data()));
            }
        }
        backend.cache().update(
            cacheUpdates, request_.ledger().sequence(), cacheOnly);
        log_.trace() << "Wrote objects";

        return more ? CallStatus::MORE : CallStatus::DONE;
    }

    void
    call(
        std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub>& stub,
        grpc::CompletionQueue& cq)
    {
        context_ = std::make_unique<grpc::ClientContext>();

        std::unique_ptr<grpc::ClientAsyncResponseReader<
            org::xrpl::rpc::v1::GetLedgerDataResponse>>
            rpc(stub->PrepareAsyncGetLedgerData(context_.get(), request_, &cq));

        rpc->StartCall();

        rpc->Finish(next_.get(), &status_, this);
    }

    std::string
    getMarkerPrefix()
    {
        if (next_->marker().size() == 0)
            return "";
        else
            return ripple::strHex(std::string{next_->marker().data()[0]});
    }

    std::string
    getLastKey()
    {
        return lastKey_;
    }
};

template <class Derived>
bool
ETLSourceImpl<Derived>::loadInitialLedger(
    uint32_t sequence,
    uint32_t numMarkers,
    bool cacheOnly)
{
    if (!stub_)
        return false;

    grpc::CompletionQueue cq;

    void* tag;

    bool ok = false;

    std::vector<AsyncCallData> calls;
    auto markers = getMarkers(numMarkers);

    for (size_t i = 0; i < markers.size(); ++i)
    {
        std::optional<ripple::uint256> nextMarker;
        if (i + 1 < markers.size())
            nextMarker = markers[i + 1];
        calls.emplace_back(sequence, markers[i], nextMarker);
    }

    log_.debug() << "Starting data download for ledger " << sequence
                 << ". Using source = " << toString();

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

        auto ptr = static_cast<AsyncCallData*>(tag);

        if (!ok)
        {
            log_.error() << "loadInitialLedger - ok is false";
            return false;
            // handle cancelled
        }
        else
        {
            log_.trace() << "Marker prefix = " << ptr->getMarkerPrefix();
            auto result = ptr->process(stub_, cq, *backend_, abort, cacheOnly);
            if (result != AsyncCallData::CallStatus::MORE)
            {
                numFinished++;
                log_.debug() << "Finished a marker. "
                             << "Current number of finished = " << numFinished;
                std::string lastKey = ptr->getLastKey();
                if (lastKey.size())
                    edgeKeys.push_back(ptr->getLastKey());
            }
            if (result == AsyncCallData::CallStatus::ERRORED)
            {
                abort = true;
            }
            if (backend_->cache().size() > progress)
            {
                log_.info() << "Downloaded " << backend_->cache().size()
                            << " records from rippled";
                progress += incr;
            }
        }
    }
    log_.info() << "Finished loadInitialLedger. cache size = "
                << backend_->cache().size();
    size_t numWrites = 0;
    if (!abort)
    {
        backend_->cache().setFull();
        if (!cacheOnly)
        {
            auto start = std::chrono::system_clock::now();
            for (auto& key : edgeKeys)
            {
                log_.debug() << "Writing edge key = " << ripple::strHex(key);
                auto succ = backend_->cache().getSuccessor(
                    *ripple::uint256::fromVoidChecked(key), sequence);
                if (succ)
                    backend_->writeSuccessor(
                        std::move(key), sequence, uint256ToString(succ->key));
            }
            ripple::uint256 prev = Backend::firstKey;
            while (auto cur = backend_->cache().getSuccessor(prev, sequence))
            {
                assert(cur);
                if (prev == Backend::firstKey)
                {
                    backend_->writeSuccessor(
                        uint256ToString(prev),
                        sequence,
                        uint256ToString(cur->key));
                }

                if (isBookDir(cur->key, cur->blob))
                {
                    auto base = getBookBase(cur->key);
                    // make sure the base is not an actual object
                    if (!backend_->cache().get(cur->key, sequence))
                    {
                        auto succ =
                            backend_->cache().getSuccessor(base, sequence);
                        assert(succ);
                        if (succ->key == cur->key)
                        {
                            log_.debug() << "Writing book successor = "
                                         << ripple::strHex(base) << " - "
                                         << ripple::strHex(cur->key);

                            backend_->writeSuccessor(
                                uint256ToString(base),
                                sequence,
                                uint256ToString(cur->key));
                        }
                    }
                    ++numWrites;
                }
                prev = std::move(cur->key);
                if (numWrites % 100000 == 0 && numWrites != 0)
                    log_.info() << "Wrote " << numWrites << " book successors";
            }

            backend_->writeSuccessor(
                uint256ToString(prev),
                sequence,
                uint256ToString(Backend::lastKey));

            ++numWrites;
            auto end = std::chrono::system_clock::now();
            auto seconds =
                std::chrono::duration_cast<std::chrono::seconds>(end - start)
                    .count();
            log_.info()
                << "Looping through cache and submitting all writes took "
                << seconds
                << " seconds. numWrites = " << std::to_string(numWrites);
        }
    }
    return !abort;
}

template <class Derived>
std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
ETLSourceImpl<Derived>::fetchLedger(
    uint32_t ledgerSequence,
    bool getObjects,
    bool getObjectNeighbors)
{
    org::xrpl::rpc::v1::GetLedgerResponse response;
    if (!stub_)
        return {{grpc::StatusCode::INTERNAL, "No Stub"}, response};

    // ledger header with txns and metadata
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
        log_.warn() << "ETLSourceImpl::fetchLedger - is_unlimited is "
                       "false. Make sure secure_gateway is set "
                       "correctly on the ETL source. source = "
                    << toString() << " status = " << status.error_message();
    }
    return {status, std::move(response)};
}

static std::unique_ptr<ETLSource>
make_ETLSource(
    clio::Config const& config,
    boost::asio::io_context& ioContext,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<NetworkValidatedLedgers> networkValidatedLedgers,
    ETLLoadBalancer& balancer)
{
    auto src = std::make_unique<ProbingETLSource>(
        config,
        ioContext,
        backend,
        subscriptions,
        networkValidatedLedgers,
        balancer);

    src->run();

    return src;
}

ETLLoadBalancer::ETLLoadBalancer(
    clio::Config const& config,
    boost::asio::io_context& ioContext,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<NetworkValidatedLedgers> nwvl)
{
    if (auto value = config.maybeValue<uint32_t>("num_markers"); value)
        downloadRanges_ = std::clamp(*value, 1u, 256u);
    else if (backend->fetchLedgerRange())
        downloadRanges_ = 4;

    for (auto const& entry : config.array("etl_sources"))
    {
        std::unique_ptr<ETLSource> source = make_ETLSource(
            entry, ioContext, backend, subscriptions, nwvl, *this);

        sources_.push_back(std::move(source));
        log_.info() << "Added etl source - " << sources_.back()->toString();
    }
}

void
ETLLoadBalancer::loadInitialLedger(uint32_t sequence, bool cacheOnly)
{
    execute(
        [this, &sequence, cacheOnly](auto& source) {
            bool res =
                source->loadInitialLedger(sequence, downloadRanges_, cacheOnly);
            if (!res)
            {
                log_.error() << "Failed to download initial ledger."
                             << " Sequence = " << sequence
                             << " source = " << source->toString();
            }
            return res;
        },
        sequence);
}

std::optional<org::xrpl::rpc::v1::GetLedgerResponse>
ETLLoadBalancer::fetchLedger(
    uint32_t ledgerSequence,
    bool getObjects,
    bool getObjectNeighbors)
{
    org::xrpl::rpc::v1::GetLedgerResponse response;
    bool success = execute(
        [&response, ledgerSequence, getObjects, getObjectNeighbors, log = log_](
            auto& source) {
            auto [status, data] = source->fetchLedger(
                ledgerSequence, getObjects, getObjectNeighbors);
            response = std::move(data);
            if (status.ok() && response.validated())
            {
                log.info() << "Successfully fetched ledger = " << ledgerSequence
                           << " from source = " << source->toString();
                return true;
            }
            else
            {
                log.warn() << "Error getting ledger = " << ledgerSequence
                           << ", Reply: " << response.DebugString()
                           << ", error_code: " << status.error_code()
                           << ", error_msg: " << status.error_message()
                           << ", source = " << source->toString();
                return false;
            }
        },
        ledgerSequence);
    if (success)
        return response;
    else
        return {};
}

std::optional<boost::json::object>
ETLLoadBalancer::forwardToRippled(
    boost::json::object const& request,
    std::string const& clientIp,
    boost::asio::yield_context& yield) const
{
    srand((unsigned)time(0));
    auto sourceIdx = rand() % sources_.size();
    auto numAttempts = 0;
    while (numAttempts < sources_.size())
    {
        if (auto res =
                sources_[sourceIdx]->forwardToRippled(request, clientIp, yield))
            return res;

        sourceIdx = (sourceIdx + 1) % sources_.size();
        ++numAttempts;
    }
    return {};
}

template <class Derived>
std::optional<boost::json::object>
ETLSourceImpl<Derived>::forwardToRippled(
    boost::json::object const& request,
    std::string const& clientIp,
    boost::asio::yield_context& yield) const
{
    if (auto resp = forwardCache_.get(request); resp)
    {
        log_.debug() << "request hit forwardCache";
        return resp;
    }

    return requestFromRippled(request, clientIp, yield);
}

template <class Derived>
std::optional<boost::json::object>
ETLSourceImpl<Derived>::requestFromRippled(
    boost::json::object const& request,
    std::string const& clientIp,
    boost::asio::yield_context& yield) const
{
    log_.trace() << "Attempting to forward request to tx. "
                 << "request = " << boost::json::serialize(request);

    boost::json::object response;
    if (!connected_)
    {
        log_.error() << "Attempted to proxy but failed to connect to tx";
        return {};
    }
    namespace beast = boost::beast;          // from <boost/beast.hpp>
    namespace http = beast::http;            // from <boost/beast/http.hpp>
    namespace websocket = beast::websocket;  // from
    namespace net = boost::asio;             // from
    using tcp = boost::asio::ip::tcp;        // from
    try
    {
        boost::beast::error_code ec;
        // These objects perform our I/O
        tcp::resolver resolver{ioc_};

        log_.trace() << "Creating websocket";
        auto ws = std::make_unique<websocket::stream<beast::tcp_stream>>(ioc_);

        // Look up the domain name
        auto const results = resolver.async_resolve(ip_, wsPort_, yield[ec]);
        if (ec)
            return {};

        ws->next_layer().expires_after(std::chrono::seconds(3));

        log_.trace() << "Connecting websocket";
        // Make the connection on the IP address we get from a lookup
        ws->next_layer().async_connect(results, yield[ec]);
        if (ec)
            return {};

        // Set a decorator to change the User-Agent of the handshake
        // and to tell rippled to charge the client IP for RPC
        // resources. See "secure_gateway" in
        //
        // https://github.com/ripple/rippled/blob/develop/cfg/rippled-example.cfg
        ws->set_option(websocket::stream_base::decorator(
            [&clientIp](websocket::request_type& req) {
                req.set(
                    http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-client-coro");
                req.set(http::field::forwarded, "for=" + clientIp);
            }));
        log_.trace() << "client ip: " << clientIp;

        log_.trace() << "Performing websocket handshake";
        // Perform the websocket handshake
        ws->async_handshake(ip_, "/", yield[ec]);
        if (ec)
            return {};

        log_.trace() << "Sending request";
        // Send the message
        ws->async_write(
            net::buffer(boost::json::serialize(request)), yield[ec]);
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
            log_.error() << "Error parsing response: "
                         << std::string{begin, end};
            return {};
        }
        log_.trace() << "Successfully forward request";

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

template <class Func>
bool
ETLLoadBalancer::execute(Func f, uint32_t ledgerSequence)
{
    srand((unsigned)time(0));
    auto sourceIdx = rand() % sources_.size();
    auto numAttempts = 0;

    while (true)
    {
        auto& source = sources_[sourceIdx];

        log_.debug() << "Attempting to execute func. ledger sequence = "
                     << ledgerSequence << " - source = " << source->toString();
        if (source->hasLedger(ledgerSequence) || true)
        {
            bool res = f(source);
            if (res)
            {
                log_.debug() << "Successfully executed func at source = "
                             << source->toString()
                             << " - ledger sequence = " << ledgerSequence;
                break;
            }
            else
            {
                log_.warn() << "Failed to execute func at source = "
                            << source->toString()
                            << " - ledger sequence = " << ledgerSequence;
            }
        }
        else
        {
            log_.warn() << "Ledger not present at source = "
                        << source->toString()
                        << " - ledger sequence = " << ledgerSequence;
        }
        sourceIdx = (sourceIdx + 1) % sources_.size();
        numAttempts++;
        if (numAttempts % sources_.size() == 0)
        {
            log_.error() << "Error executing function "
                         << " - ledger sequence = " << ledgerSequence
                         << " - Tried all sources. Sleeping and trying again";
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    return true;
}
