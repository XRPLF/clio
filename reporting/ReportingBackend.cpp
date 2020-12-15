#include <reporting/ReportingBackend.h>
// Process the result of an asynchronous write. Retry on error
// @param fut cassandra future associated with the write
// @param cbData struct that holds the request parameters
void
flatMapWriteCallback(CassFuture* fut, void* cbData)
{
    CassandraFlatMapBackend::WriteCallbackData& requestParams =
        *static_cast<CassandraFlatMapBackend::WriteCallbackData*>(cbData);
    CassandraFlatMapBackend& backend = *requestParams.backend;
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
        delete &requestParams;
    }
}
void
flatMapWriteTransactionCallback(CassFuture* fut, void* cbData)
{
    CassandraFlatMapBackend::WriteTransactionCallbackData& requestParams =
        *static_cast<CassandraFlatMapBackend::WriteTransactionCallbackData*>(
            cbData);
    CassandraFlatMapBackend& backend = *requestParams.backend;
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

// Process the result of an asynchronous read. Retry on error
// @param fut cassandra future associated with the read
// @param cbData struct that holds the request parameters
void
flatMapReadCallback(CassFuture* fut, void* cbData)
{
    CassandraFlatMapBackend::ReadCallbackData& requestParams =
        *static_cast<CassandraFlatMapBackend::ReadCallbackData*>(cbData);

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
        requestParams.result = std::make_pair(std::move(txn), std::move(meta));
        cass_result_free(res);
        finish();
    }
}
