#include <ripple/beast/net/IPEndpoint.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <boost/asio/strand.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/json.hpp>
#include <boost/json/src.hpp>
#include <boost/log/trivial.hpp>
#include <etl/ETLSource.h>
#include <etl/ReportingETL.h>

// Create ETL source without grpc endpoint
// Fetch ledger and load initial ledger will fail for this source
// Primarly used in read-only mode, to monitor when ledgers are validated
template <class Derived>
ETLSourceImpl<Derived>::ETLSourceImpl(
    boost::json::object const& config,
    boost::asio::io_context& ioContext,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<NetworkValidatedLedgers> networkValidatedLedgers,
    ETLLoadBalancer& balancer)
    : ioc_(ioContext)
    , resolver_(boost::asio::make_strand(ioContext))
    , timer_(ioContext)
    , networkValidatedLedgers_(networkValidatedLedgers)
    , backend_(backend)
    , subscriptions_(subscriptions)
    , balancer_(balancer)
{

    if (config.contains("ip"))
    {
        auto ipJs = config.at("ip").as_string();
        ip_ = {ipJs.c_str(), ipJs.size()};
    }
    if (config.contains("ws_port"))
    {
        auto portjs = config.at("ws_port").as_string();
        wsPort_ = {portjs.c_str(), portjs.size()};
    }
    if (config.contains("grpc_port"))
    {
        auto portjs = config.at("grpc_port").as_string();
        grpcPort_ = {portjs.c_str(), portjs.size()};
        try
        {
            boost::asio::ip::tcp::endpoint endpoint{
                boost::asio::ip::make_address(ip_), std::stoi(grpcPort_)};
            std::stringstream ss;
            ss << endpoint;
            stub_ = org::xrpl::rpc::v1::XRPLedgerAPIService::NewStub(
                grpc::CreateChannel(
                    ss.str(), grpc::InsecureChannelCredentials()));
            BOOST_LOG_TRIVIAL(debug) << "Made stub for remote = " << toString();
        }
        catch (std::exception const& e)
        {
            BOOST_LOG_TRIVIAL(debug)
                << "Exception while creating stub = " << e.what()
                << " . Remote = " << toString();
        }
    }
}

template <class Derived>
void
ETLSourceImpl<Derived>::reconnect(boost::beast::error_code ec)
{
    connected_ = false;
    // These are somewhat normal errors. operation_aborted occurs on shutdown,
    // when the timer is cancelled. connection_refused will occur repeatedly
    std::string err = ec.message();
    // if we cannot connect to the transaction processing process
    if (ec.category() == boost::asio::error::get_ssl_category()) {
        err = std::string(" (")
                +boost::lexical_cast<std::string>(ERR_GET_LIB(ec.value()))+","
                +boost::lexical_cast<std::string>(ERR_GET_FUNC(ec.value()))+","
                +boost::lexical_cast<std::string>(ERR_GET_REASON(ec.value()))+") "
        ;
        //ERR_PACK /* crypto/err/err.h */
        char buf[128];
        ::ERR_error_string_n(ec.value(), buf, sizeof(buf));
        err += buf;

        std::cout << err << std::endl;
    }

    if (ec != boost::asio::error::operation_aborted &&
        ec != boost::asio::error::connection_refused)
    {
        BOOST_LOG_TRIVIAL(error)
            << __func__ << " : "
            << "error code = " << ec << " - " << toString();
    }
    else
    {
        BOOST_LOG_TRIVIAL(warning)
            << __func__ << " : "
            << "error code = " << ec << " - " << toString();
    }

    // exponentially increasing timeouts, with a max of 30 seconds
    size_t waitTime = std::min(pow(2, numFailures_), 30.0);
    numFailures_++;
    timer_.expires_after(boost::asio::chrono::seconds(waitTime));
    timer_.async_wait([this](auto ec) {
        bool startAgain = (ec != boost::asio::error::operation_aborted);
        BOOST_LOG_TRIVIAL(trace) << __func__ << " async_wait : ec = " << ec;
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
                        BOOST_LOG_TRIVIAL(error)
                            << __func__ << " async_close : "
                            << "error code = " << ec << " - " << toString();
                    }
                    closing_ = false;
                    if (startAgain)
                        run();
                });
        }
        else if (startAgain)
        {
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
                        BOOST_LOG_TRIVIAL(error)
                            << __func__ << " async_close : "
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
                            boost::beast::ssl_stream<
                            boost::beast::tcp_stream>>>(
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
    BOOST_LOG_TRIVIAL(trace)
        << __func__ << " : ec = " << ec << " - " << toString();
    if (ec)
    {
        // try again
        reconnect(ec);
    }
    else
    {
        boost::beast::get_lowest_layer(derived().ws()).expires_after(
            std::chrono::seconds(30));
        boost::beast::get_lowest_layer(derived().ws()).async_connect(
            results, [this](auto ec, auto ep) { derived().onConnect(ec, ep); });
    }
}

void
PlainETLSource::onConnect(
    boost::beast::error_code ec,
    boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    BOOST_LOG_TRIVIAL(trace)
        << __func__ << " : ec = " << ec << " - " << toString();
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

        // Set suggested timeout settings for the websocket
        derived().ws().set_option(
            boost::beast::websocket::stream_base::timeout::suggested(
                boost::beast::role_type::client));

        // Set a decorator to change the User-Agent of the handshake
        derived().ws().set_option(boost::beast::websocket::stream_base::decorator(
            [](boost::beast::websocket::request_type& req) {
                req.set(
                    boost::beast::http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-client-async");
            }));

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        auto host = ip_ + ':' + std::to_string(endpoint.port());
        // Perform the websocket handshake
        derived().ws().async_handshake(host, "/", [this](auto ec) { onHandshake(ec); });
    }
}

void
SslETLSource::onConnect(
    boost::beast::error_code ec,
    boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint)
{
    BOOST_LOG_TRIVIAL(trace)
        << __func__ << " : ec = " << ec << " - " << toString();
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

        // Set suggested timeout settings for the websocket
        derived().ws().set_option(
            boost::beast::websocket::stream_base::timeout::suggested(
                boost::beast::role_type::client));

        // Set a decorator to change the User-Agent of the handshake
        derived().ws().set_option(boost::beast::websocket::stream_base::decorator(
            [](boost::beast::websocket::request_type& req) {
                req.set(
                    boost::beast::http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-client-async");
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
    BOOST_LOG_TRIVIAL(trace)
        << __func__ << " : ec = " << ec << " - " << toString();
    if (ec)
    {
        // start over
        reconnect(ec);
    }
    else
    {
        boost::json::object jv{
            {"command", "subscribe"},
            {"streams", {"ledger", "transactions_proposed"}}};
        std::string s = boost::json::serialize(jv);
        BOOST_LOG_TRIVIAL(trace) << "Sending subscribe stream message";
        // Send the message
        derived().ws().async_write(boost::asio::buffer(s), [this](auto ec, size_t size) {
            onWrite(ec, size);
        });
    }
}

template <class Derived>
void
ETLSourceImpl<Derived>::onWrite(boost::beast::error_code ec, size_t bytesWritten)
{
    BOOST_LOG_TRIVIAL(trace)
        << __func__ << " : ec = " << ec << " - " << toString();
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
    BOOST_LOG_TRIVIAL(trace)
        << __func__ << " : ec = " << ec << " - " << toString();
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

        BOOST_LOG_TRIVIAL(trace)
            << __func__ << " : calling async_read - " << toString();
         derived().ws().async_read(
            readBuffer_, [this](auto ec, size_t size) { onRead(ec, size); });
    }
}

template <class Derived>
bool
ETLSourceImpl<Derived>::handleMessage()
{
    BOOST_LOG_TRIVIAL(trace) << __func__ << " : " << toString();

    setLastMsgTime();
    connected_ = true;
    try
    {
        std::string msg{
            static_cast<char const*>(readBuffer_.data().data()),
            readBuffer_.size()};
        // BOOST_LOG_TRIVIAL(debug) << __func__ << msg;
        boost::json::value raw = boost::json::parse(msg);
        // BOOST_LOG_TRIVIAL(debug) << __func__ << " parsed";
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
            BOOST_LOG_TRIVIAL(debug)
                << __func__ << " : "
                << "Received a message on ledger "
                << " subscription stream. Message : " << response << " - "
                << toString();
        }
        else
        {
            if (response.contains("transaction"))
            {
                if (balancer_.shouldPropagateTxnStream(this))
                {
                    subscriptions_->forwardProposedTransaction(response);
                }
            }
            else
            {
                BOOST_LOG_TRIVIAL(debug)
                    << __func__ << " : "
                    << "Received a message on ledger "
                    << " subscription stream. Message : " << response << " - "
                    << toString();
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
        }

        if (ledgerIndex != 0)
        {
            BOOST_LOG_TRIVIAL(trace)
                << __func__ << " : "
                << "Pushing ledger sequence = " << ledgerIndex << " - "
                << toString();
            networkValidatedLedgers_->push(ledgerIndex);
        }
        return true;
    }
    catch (std::exception const& e)
    {
        BOOST_LOG_TRIVIAL(error) << "Exception in handleMessage : " << e.what();
        return false;
    }
}

class AsyncCallData
{
    std::unique_ptr<org::xrpl::rpc::v1::GetLedgerDataResponse> cur_;
    std::unique_ptr<org::xrpl::rpc::v1::GetLedgerDataResponse> next_;

    org::xrpl::rpc::v1::GetLedgerDataRequest request_;
    std::unique_ptr<grpc::ClientContext> context_;

    grpc::Status status_;

    unsigned char nextPrefix_;
public:
    AsyncCallData(
        uint32_t seq,
        ripple::uint256& marker,
        std::optional<ripple::uint256> nextMarker)
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

        BOOST_LOG_TRIVIAL(debug)
            << "Setting up AsyncCallData. marker = " << ripple::strHex(marker)
            << " . prefix = " << ripple::strHex(std::string(1, prefix))
            << " . nextPrefix_ = " << ripple::strHex(std::string(1, nextPrefix_));

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
        BackendInterface const& backend,
        bool abort = false)
    {
        BOOST_LOG_TRIVIAL(debug) << "Processing calldata";
        if (abort)
        {
            BOOST_LOG_TRIVIAL(error) << "AsyncCallData aborted";
            return CallStatus::ERRORED;
        }
        if (!status_.ok())
        {
            BOOST_LOG_TRIVIAL(debug) << "AsyncCallData status_ not ok: "
                                   << " code = " << status_.error_code()
                                   << " message = " << status_.error_message();
            return CallStatus::ERRORED;
        }
        if (!next_->is_unlimited())
        {
            BOOST_LOG_TRIVIAL(warning)
                << "AsyncCallData is_unlimited is false. Make sure "
                   "secure_gateway is set correctly at the ETL source";
            assert(false);
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

        for (auto& obj : *(cur_->mutable_ledger_objects()->mutable_objects()))
        {
            backend.writeLedgerObject(
                std::move(*obj.mutable_key()),
                request_.ledger().sequence(),
                std::move(*obj.mutable_data()));
        }

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
};

template <class Derived>
bool
ETLSourceImpl<Derived>::loadInitialLedger(
    uint32_t sequence,
    uint32_t numMarkers)
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

    BOOST_LOG_TRIVIAL(debug) << "Starting data download for ledger " << sequence
                           << ". Using source = " << toString();

    for (auto& c : calls)
        c.call(stub_, cq);

    size_t numFinished = 0;
    bool abort = false;
    while (numFinished < calls.size() &&
           cq.Next(&tag, &ok))
    {
        assert(tag);

        auto ptr = static_cast<AsyncCallData*>(tag);

        if (!ok)
        {
            BOOST_LOG_TRIVIAL(error) << "loadInitialLedger - ok is false";
            return false;
            // handle cancelled
        }
        else
        {
            BOOST_LOG_TRIVIAL(debug)
                << "Marker prefix = " << ptr->getMarkerPrefix();
            auto result = ptr->process(stub_, cq, *backend_, abort);
            if (result != AsyncCallData::CallStatus::MORE)
            {
                numFinished++;
                BOOST_LOG_TRIVIAL(debug)
                    << "Finished a marker. "
                    << "Current number of finished = " << numFinished;
            }
            if (result == AsyncCallData::CallStatus::ERRORED)
            {
                abort = true;
            }
        }
    }
    return !abort;
}


template <class Derived>
std::pair<grpc::Status, org::xrpl::rpc::v1::GetLedgerResponse>
ETLSourceImpl<Derived>::fetchLedger(uint32_t ledgerSequence, bool getObjects)
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
    request.set_user("ETL");
    grpc::Status status = stub_->GetLedger(&context, request, &response);
    if (status.ok() && !response.is_unlimited())
    {
        BOOST_LOG_TRIVIAL(warning)
            << "ETLSourceImpl::fetchLedger - is_unlimited is "
               "false. Make sure secure_gateway is set "
               "correctly on the ETL source. source = "
            << toString() << " status = " << status.error_message();
        assert(false);
    }
    return {status, std::move(response)};
}

ETLLoadBalancer::ETLLoadBalancer(
    boost::json::object const& config,
    boost::asio::io_context& ioContext,
    std::optional<std::reference_wrapper<boost::asio::ssl::context>> sslCtx,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<SubscriptionManager> subscriptions,
    std::shared_ptr<NetworkValidatedLedgers> nwvl)
{
    if (config.contains("download_ranges") &&
        config.at("download_ranges").is_int64())
    {
        downloadRanges_ = config.at("download_ranges").as_int64();

        downloadRanges_ = std::clamp(downloadRanges_, {1}, {256});
    }

    for (auto& entry : config.at("etl_sources").as_array())
    {
        std::unique_ptr<ETLSource> source = ETL::make_ETLSource(
            entry.as_object(),
            ioContext,
            sslCtx,
            backend,
            subscriptions,
            nwvl,
            *this);

        sources_.push_back(std::move(source));
        BOOST_LOG_TRIVIAL(info) << __func__ << " : added etl source - "
                                << sources_.back()->toString();
    }
}

void
ETLLoadBalancer::loadInitialLedger(uint32_t sequence)
{
    execute(
        [this, &sequence](auto& source) {
            bool res = 
                source->loadInitialLedger(sequence, downloadRanges_);
            if (!res)
            {
                BOOST_LOG_TRIVIAL(error) << "Failed to download initial ledger."
                                         << " Sequence = " << sequence
                                         << " source = " << source->toString();
            }
            return res;
        },
        sequence);
}

std::optional<org::xrpl::rpc::v1::GetLedgerResponse>
ETLLoadBalancer::fetchLedger(uint32_t ledgerSequence, bool getObjects)
{
    org::xrpl::rpc::v1::GetLedgerResponse response;
    bool success = execute(
        [&response, ledgerSequence, getObjects, this](auto& source) {
            auto [status, data] =
                source->fetchLedger(ledgerSequence, getObjects);
            response = std::move(data);
            if (status.ok() && (response.validated() || true))
            {
                BOOST_LOG_TRIVIAL(info)
                    << "Successfully fetched ledger = " << ledgerSequence
                    << " from source = " << source->toString();
                return true;
            }
            else
            {
                BOOST_LOG_TRIVIAL(warning)
                    << "Error getting ledger = " << ledgerSequence
                    << " Reply : " << response.DebugString()
                    << " error_code : " << status.error_code()
                    << " error_msg : " << status.error_message()
                    << " source = " << source->toString();
                return false;
            }
        },
        ledgerSequence);
    if (success)
        return response;
    else
        return {};
}

std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub>
ETLLoadBalancer::getRippledForwardingStub() const
{
    if (sources_.size() == 0)
        return nullptr;
    srand((unsigned)time(0));
    auto sourceIdx = rand() % sources_.size();
    auto numAttempts = 0;
    while (numAttempts < sources_.size())
    {
        auto stub = sources_[sourceIdx]->getRippledForwardingStub();
        if (!stub)
        {
            sourceIdx = (sourceIdx + 1) % sources_.size();
            ++numAttempts;
            continue;
        }
        return stub;
    }
    return nullptr;
}

std::optional<boost::json::object>
ETLLoadBalancer::forwardToRippled(boost::json::object const& request) const
{
    srand((unsigned)time(0));
    auto sourceIdx = rand() % sources_.size();
    auto numAttempts = 0;
    while (numAttempts < sources_.size())
    {
        if (auto res = sources_[sourceIdx]->forwardToRippled(request))
            return res;

        sourceIdx = (sourceIdx + 1) % sources_.size();
        ++numAttempts;
    }
    return {};
}

template <class Derived>
std::unique_ptr<org::xrpl::rpc::v1::XRPLedgerAPIService::Stub>
ETLSourceImpl<Derived>::getRippledForwardingStub() const
{
    if (!connected_)
        return nullptr;
    try
    {
        return org::xrpl::rpc::v1::XRPLedgerAPIService::NewStub(
            grpc::CreateChannel(
                beast::IP::Endpoint(
                    boost::asio::ip::make_address(ip_), std::stoi(grpcPort_))
                    .to_string(),
                grpc::InsecureChannelCredentials()));
    }
    catch (std::exception const&)
    {
        BOOST_LOG_TRIVIAL(error) << "Failed to create grpc stub";
        return nullptr;
    }
}

template <class Derived>
std::optional<boost::json::object>
ETLSourceImpl<Derived>::forwardToRippled(boost::json::object const& request) const
{
    BOOST_LOG_TRIVIAL(debug) << "Attempting to forward request to tx. "
                             << "request = " << boost::json::serialize(request);

    boost::json::object response;
    if (!connected_)
    {
        BOOST_LOG_TRIVIAL(error)
            << "Attempted to proxy but failed to connect to tx";
        return {};
    }
    namespace beast = boost::beast;          // from <boost/beast.hpp>
    namespace http = beast::http;            // from <boost/beast/http.hpp>
    namespace websocket = beast::websocket;  // from
    namespace net = boost::asio;             // from
    using tcp = boost::asio::ip::tcp;        // from
    try
    {
        // The io_context is required for all I/O
        net::io_context ioc;

        // These objects perform our I/O
        tcp::resolver resolver{ioc};

        BOOST_LOG_TRIVIAL(debug) << "Creating websocket";
        auto ws = std::make_unique<websocket::stream<tcp::socket>>(ioc);

        // Look up the domain name
        auto const results = resolver.resolve(ip_, wsPort_);

        BOOST_LOG_TRIVIAL(debug) << "Connecting websocket";
        // Make the connection on the IP address we get from a lookup
        net::connect(ws->next_layer(), results.begin(), results.end());

        // Set a decorator to change the User-Agent of the handshake
        // and to tell rippled to charge the client IP for RPC
        // resources. See "secure_gateway" in
        //
        // https://github.com/ripple/rippled/blob/develop/cfg/rippled-example.cfg
        ws->set_option(websocket::stream_base::decorator(
            [&request](websocket::request_type& req) {
                req.set(
                    http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-client-coro");
                req.set(
                    http::field::forwarded,
                    "for=" + boost::json::serialize(request));
            }));
        BOOST_LOG_TRIVIAL(debug)
            << "client ip: " << boost::json::serialize(request);

        BOOST_LOG_TRIVIAL(debug) << "Performing websocket handshake";
        // Perform the websocket handshake
        ws->handshake(ip_, "/");

        BOOST_LOG_TRIVIAL(debug) << "Sending request";
        // Send the message
        ws->write(net::buffer(boost::json::serialize(request)));

        beast::flat_buffer buffer;
        ws->read(buffer);

        auto begin = static_cast<char const*>(buffer.data().data());
        auto end = begin + buffer.data().size();
        auto parsed = boost::json::parse(std::string(begin, end));

        if (!parsed.is_object())
        {
            BOOST_LOG_TRIVIAL(error)
                << "Error parsing response: " << std::string{begin, end};
            return {};
        }
        BOOST_LOG_TRIVIAL(debug) << "Successfully forward request";

        response = parsed.as_object();

        response["forwarded"] = true;
        return response;
    }
    catch (std::exception const& e)
    {
        BOOST_LOG_TRIVIAL(error) << "Encountered exception : " << e.what();
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

        BOOST_LOG_TRIVIAL(debug)
            << __func__ << " : "
            << "Attempting to execute func. ledger sequence = "
            << ledgerSequence << " - source = " << source->toString();
        if (source->hasLedger(ledgerSequence) || true)
        {
            bool res = f(source);
            if (res)
            {
                BOOST_LOG_TRIVIAL(debug)
                    << __func__ << " : "
                    << "Successfully executed func at source = "
                    << source->toString()
                    << " - ledger sequence = " << ledgerSequence;
                break;
            }
            else
            {
                BOOST_LOG_TRIVIAL(warning)
                    << __func__ << " : "
                    << "Failed to execute func at source = "
                    << source->toString()
                    << " - ledger sequence = " << ledgerSequence;
            }
        }
        else
        {
            BOOST_LOG_TRIVIAL(warning)
                << __func__ << " : "
                << "Ledger not present at source = " << source->toString()
                << " - ledger sequence = " << ledgerSequence;
        }
        sourceIdx = (sourceIdx + 1) % sources_.size();
        numAttempts++;
        if (numAttempts % sources_.size() == 0)
        {
            BOOST_LOG_TRIVIAL(error)
                << __func__ << " : "
                << "Error executing function "
                << " - ledger sequence = " << ledgerSequence
                << " - Tried all sources. Sleeping and trying again";
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
    return true;
}

