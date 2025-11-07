//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "data/BackendInterface.hpp"
#include "etl/MonitorInterface.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "util/Mutex.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/async/AnyStrand.hpp"
#include "util/log/Logger.hpp"

#include <boost/signals2/connection.hpp>
#include <xrpl/protocol/TxFormats.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace etl::impl {

class Monitor : public MonitorInterface {
    util::async::AnyStrand strand_;
    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers_;

    std::atomic_uint32_t nextSequence_;
    std::optional<util::async::AnyOperation<void>> repeatedTask_;
    std::optional<boost::signals2::scoped_connection> subscription_;  // network validated ledgers subscription

    NewSequenceSignalType notificationChannel_;
    DbStalledSignalType dbStalledChannel_;

    struct UpdateData {
        std::chrono::steady_clock::duration dbStalledReportDelay;
        std::chrono::steady_clock::time_point lastDbCheckTime;
        uint32_t lastSeenMaxSeqInDb = 0u;
    };

    util::Mutex<UpdateData> updateData_;

    util::Logger log_{"ETL"};

public:
    Monitor(
        util::async::AnyExecutionContext ctx,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
        uint32_t startSequence,
        std::chrono::steady_clock::duration dbStalledReportDelay
    );
    ~Monitor() override;

    void
    notifySequenceLoaded(uint32_t seq) override;

    void
    notifyWriteConflict(uint32_t seq) override;

    void
    run(std::chrono::steady_clock::duration repeatInterval) override;

    void
    stop() override;

    boost::signals2::scoped_connection
    subscribeToNewSequence(NewSequenceSignalType::slot_type const& subscriber) override;

    boost::signals2::scoped_connection
    subscribeToDbStalled(DbStalledSignalType::slot_type const& subscriber) override;

private:
    void
    onNextSequence(uint32_t seq);

    void
    doWork();
};

}  // namespace etl::impl
