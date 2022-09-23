#ifndef CLIO_WORK_QUEUE_H
#define CLIO_WORK_QUEUE_H

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>

#include <memory>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <thread>

class WorkQueue
{
    // these are cumulative for the lifetime of the process
    std::atomic_uint64_t queued_ = 0;
    std::atomic_uint64_t durationUs_ = 0;

    std::atomic_uint64_t curSize_ = 0;
    uint32_t maxSize_ = std::numeric_limits<uint32_t>::max();

public:
    WorkQueue(std::uint32_t numWorkers, uint32_t maxSize = 0);

    template <typename F>
    bool
    postCoro(F&& f, bool isWhiteListed)
    {
        if (curSize_ >= maxSize_ && !isWhiteListed)
        {
            BOOST_LOG_TRIVIAL(warning)
                << __func__
                << " queue is full. rejecting job. current size = " << curSize_
                << " max size = " << maxSize_;
            return false;
        }
        ++curSize_;
        auto start = std::chrono::system_clock::now();
        // Each time we enqueue a job, we want to post a symmetrical job that
        // will dequeue and run the job at the front of the job queue.
        boost::asio::spawn(
            ioc_,
            [this, f = std::move(f), start](boost::asio::yield_context yield) {
                auto run = std::chrono::system_clock::now();
                auto wait =
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        run - start)
                        .count();
                // increment queued_ here, in the same place we implement
                // durationUs_
                ++queued_;
                durationUs_ += wait;
                BOOST_LOG_TRIVIAL(debug) << "WorkQueue wait time = " << wait
                                         << " queue size = " << curSize_;
                f(yield);
                --curSize_;
            });
        return true;
    }

    boost::json::object
    report() const
    {
        boost::json::object obj;
        obj["queued"] = queued_;
        obj["queued_duration_us"] = durationUs_;
        obj["current_queue_size"] = curSize_;
        obj["max_queue_size"] = maxSize_;
        return obj;
    }

private:
    std::vector<std::thread> threads_ = {};

    boost::asio::io_context ioc_ = {};
    std::optional<boost::asio::io_context::work> work_{ioc_};
};

#endif  // CLIO_WORK_QUEUE_H
