#include "etl/impl/Monitor.hpp"

#include "data/BackendInterface.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "util/Assert.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/log/Logger.hpp"

#include <boost/signals2/connection.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace etl::impl {
Monitor::Monitor(
    util::async::AnyExecutionContext ctx,
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
    uint32_t startSequence,
    std::chrono::steady_clock::duration dbStalledReportDelay
)
    : strand_(ctx.makeStrand())
    , backend_(std::move(backend))
    , validatedLedgers_(std::move(validatedLedgers))
    , nextSequence_(startSequence)
    , updateData_({
          .dbStalledReportDelay = dbStalledReportDelay,
          .lastDbCheckTime = std::chrono::steady_clock::now(),
          .lastSeenMaxSeqInDb = startSequence > 0 ? startSequence - 1 : 0,
      })
{
}

Monitor::~Monitor()
{
    stop();
}

void
Monitor::notifySequenceLoaded(uint32_t seq)
{
    LOG(log_.debug()) << "Loader notified Monitor about newly committed ledger " << seq;
    {
        auto lck = updateData_.lock();
        lck->lastSeenMaxSeqInDb = std::max(seq, lck->lastSeenMaxSeqInDb);
        lck->lastDbCheckTime = std::chrono::steady_clock::now();
    }
    repeatedTask_->invoke();  // force-invoke doWork immediately
};

void
Monitor::notifyWriteConflict(uint32_t seq)
{
    LOG(log_.warn()) << "Loader notified Monitor about write conflict at " << seq;
    nextSequence_ =
        seq + 1;  //  we already loaded the cache for seq just before we detected conflict
    LOG(log_.warn()) << "Resume monitoring from " << nextSequence_;
}

void
Monitor::run(std::chrono::steady_clock::duration repeatInterval)
{
    ASSERT(not repeatedTask_.has_value(), "Monitor attempted to run more than once");
    {
        auto lck = updateData_.lock();
        LOG(
            log_.debug()
        ) << "Starting monitor with repeat interval: "
          << std::chrono::duration_cast<std::chrono::seconds>(repeatInterval).count()
          << "s and dbStalledReportDelay: "
          << std::chrono::duration_cast<std::chrono::seconds>(lck->dbStalledReportDelay).count()
          << "s";
    }

    repeatedTask_ =
        strand_.executeRepeatedly(repeatInterval, std::bind_front(&Monitor::doWork, this));
    subscription_ = validatedLedgers_->subscribe(std::bind_front(&Monitor::onNextSequence, this));
}

void
Monitor::stop()
{
    if (repeatedTask_.has_value())
        repeatedTask_->abort();

    subscription_ = std::nullopt;
    repeatedTask_ = std::nullopt;
}

boost::signals2::scoped_connection
Monitor::subscribeToNewSequence(NewSequenceSignalType::slot_type const& subscriber)
{
    return notificationChannel_.connect(subscriber);
}

boost::signals2::scoped_connection
Monitor::subscribeToDbStalled(DbStalledSignalType::slot_type const& subscriber)
{
    return dbStalledChannel_.connect(subscriber);
}

void
Monitor::onNextSequence(uint32_t seq)
{
    ASSERT(repeatedTask_.has_value(), "Ledger subscription without repeated task is a logic error");
    LOG(log_.debug()) << "Notified about new sequence on the network: " << seq;
    repeatedTask_->invoke();  // force-invoke immediately
}

void
Monitor::doWork()
{
    auto rng = backend_->hardFetchLedgerRangeNoThrow();
    bool dbProgressedThisCycle = false;
    auto lck = updateData_.lock();

    if (rng.has_value()) {
        if (rng->maxSequence > lck->lastSeenMaxSeqInDb) {
            LOG(log_.trace()) << "DB progressed. Old max seq = " << lck->lastSeenMaxSeqInDb
                              << ", new max seq = " << rng->maxSequence;
            lck->lastSeenMaxSeqInDb = rng->maxSequence;
            dbProgressedThisCycle = true;
        }

        while (lck->lastSeenMaxSeqInDb >= nextSequence_) {
            LOG(log_.trace()) << "Publishing from Monitor::doWork. nextSequence_ = "
                              << nextSequence_
                              << ", lastSeenMaxSeqInDb_ = " << lck->lastSeenMaxSeqInDb;
            notificationChannel_(nextSequence_++);
            dbProgressedThisCycle = true;
        }
    } else {
        LOG(log_.trace()) << "DB range is not available or empty. lastSeenMaxSeqInDb_ = "
                          << lck->lastSeenMaxSeqInDb << ", nextSequence_ = " << nextSequence_;
    }

    if (dbProgressedThisCycle) {
        lck->lastDbCheckTime = std::chrono::steady_clock::now();
    } else if (
        std::chrono::steady_clock::now() - lck->lastDbCheckTime > lck->dbStalledReportDelay
    ) {
        LOG(
            log_.info()
        ) << "No DB update detected for "
          << std::chrono::duration_cast<std::chrono::seconds>(lck->dbStalledReportDelay).count()
          << " seconds. Firing dbStalledChannel. Last seen max seq in DB: "
          << lck->lastSeenMaxSeqInDb << ". Expecting next: " << nextSequence_;
        dbStalledChannel_();
        lck->lastDbCheckTime = std::chrono::steady_clock::now();
    }
}

}  // namespace etl::impl
