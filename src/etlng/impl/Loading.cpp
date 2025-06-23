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

#include "etlng/impl/Loading.hpp"

#include "data/BackendInterface.hpp"
#include "etl/SystemState.hpp"
#include "etl/impl/LedgerLoader.hpp"
#include "etlng/AmendmentBlockHandlerInterface.hpp"
#include "etlng/LoaderInterface.hpp"
#include "etlng/Models.hpp"
#include "etlng/RegistryInterface.hpp"
#include "util/Assert.hpp"
#include "util/Constants.hpp"
#include "util/LedgerUtils.hpp"
#include "util/Profiler.hpp"
#include "util/log/Logger.hpp"

#include <xrpl/protocol/LedgerHeader.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace etlng::impl {

Loader::Loader(
    std::shared_ptr<BackendInterface> backend,
    std::shared_ptr<RegistryInterface> registry,
    std::shared_ptr<AmendmentBlockHandlerInterface> amendmentBlockHandler,
    std::shared_ptr<etl::SystemState> state
)
    : backend_(std::move(backend))
    , registry_(std::move(registry))
    , amendmentBlockHandler_(std::move(amendmentBlockHandler))
    , state_(std::move(state))
{
}

std::expected<void, LoaderError>
Loader::load(model::LedgerData const& data)
{
    try {
        // Perform cache updates and all writes from extensions
        // TODO: maybe this readonly logic should be removed?
        registry_->dispatch(data);

        // Only a writer should attempt to commit to DB
        // This is also where conflicts with other writer nodes will be detected
        if (state_->isWriting) {
            auto [success, duration] =
                ::util::timed<std::chrono::milliseconds>([&]() { return backend_->finishWrites(data.seq); });
            LOG(log_.info()) << "Finished writes to DB for " << data.seq << ": " << (success ? "YES" : "NO")
                             << "; took " << duration << "ms";

            if (not success) {
                state_->writeConflict = true;
                LOG(log_.warn()) << "Another node wrote a ledger into the DB - we have a write conflict";
                return std::unexpected(LoaderError::WriteConflict);
            }
        }
    } catch (std::runtime_error const& e) {
        LOG(log_.fatal()) << "Failed to load " << data.seq << ": " << e.what();
        amendmentBlockHandler_->notifyAmendmentBlocked();
        return std::unexpected(LoaderError::AmendmentBlocked);
    }

    return {};
};

void
Loader::onInitialLoadGotMoreObjects(
    uint32_t seq,
    std::vector<model::Object> const& data,
    std::optional<std::string> lastKey
)
{
    static constexpr std::size_t kLOG_STRIDE = 1000u;
    static auto kINITIAL_LOAD_START_TIME = std::chrono::steady_clock::now();

    try {
        LOG(log_.trace()) << "On initial load: got more objects for seq " << seq << ". size = " << data.size();
        registry_->dispatchInitialObjects(
            seq,
            data,
            std::move(lastKey).value_or(std::string{})  // TODO: perhaps use optional all the way to extensions?
        );

        initialLoadWrittenObjects_ += data.size();
        ++initialLoadWrites_;
        if (initialLoadWrites_ % kLOG_STRIDE == 0u && initialLoadWrites_ != 0u) {
            auto elapsedSinceStart = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - kINITIAL_LOAD_START_TIME
            );
            auto elapsedSeconds = elapsedSinceStart.count() / static_cast<double>(util::kMILLISECONDS_PER_SECOND);
            auto objectsPerSecond =
                elapsedSeconds > 0.0 ? static_cast<double>(initialLoadWrittenObjects_) / elapsedSeconds : 0.0;

            LOG(log_.info()) << "Wrote " << initialLoadWrittenObjects_
                             << " initial ledger objects so far with average rate of " << objectsPerSecond
                             << " objects per second";
        }

    } catch (std::runtime_error const& e) {
        LOG(log_.fatal()) << "Failed to load initial objects for " << seq << ": " << e.what();
        amendmentBlockHandler_->notifyAmendmentBlocked();
    }
}

std::optional<ripple::LedgerHeader>
Loader::loadInitialLedger(model::LedgerData const& data)
{
    try {
        if (auto const rng = backend_->hardFetchLedgerRangeNoThrow(); rng.has_value()) {
            ASSERT(false, "Database is not empty");
            return std::nullopt;
        }

        LOG(log_.debug()) << "Deserialized ledger header. " << ::util::toString(data.header);

        auto seconds = ::util::timed<std::chrono::seconds>([this, &data]() { registry_->dispatchInitialData(data); });
        LOG(log_.info()) << "Dispatching initial data and submitting all writes took " << seconds << " seconds.";

        backend_->finishWrites(data.seq);
        LOG(log_.debug()) << "Loaded initial ledger";

        return {data.header};
    } catch (std::runtime_error const& e) {
        LOG(log_.fatal()) << "Failed to load initial ledger " << data.seq << ": " << e.what();
        amendmentBlockHandler_->notifyAmendmentBlocked();
        return std::nullopt;
    }
}

}  // namespace etlng::impl
