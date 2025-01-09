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
#include "etl/LedgerFetcherInterface.hpp"
#include "etl/impl/LedgerLoader.hpp"
#include "etlng/AmendmentBlockHandlerInterface.hpp"
#include "etlng/Models.hpp"
#include "etlng/RegistryInterface.hpp"
#include "util/Assert.hpp"
#include "util/LedgerUtils.hpp"
#include "util/Profiler.hpp"
#include "util/log/Logger.hpp"

#include <org/xrpl/rpc/v1/ledger.pb.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/proto/org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STTx.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/TxMeta.h>

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
    std::shared_ptr<etl::LedgerFetcherInterface> fetcher,
    std::shared_ptr<RegistryInterface> registry,
    std::shared_ptr<AmendmentBlockHandlerInterface> amendmentBlockHandler
)
    : backend_(std::move(backend))
    , fetcher_(std::move(fetcher))
    , registry_(std::move(registry))
    , amendmentBlockHandler_(std::move(amendmentBlockHandler))
{
}

void
Loader::load(model::LedgerData const& data)
{
    try {
        // perform cache updates and all writes from extensions
        registry_->dispatch(data);

        auto [success, duration] =
            ::util::timed<std::chrono::duration<double>>([&]() { return backend_->finishWrites(data.seq); });
        LOG(log_.info()) << "Finished writes to DB for " << data.seq << ": " << (success ? "YES" : "NO") << "; took "
                         << duration;
    } catch (std::runtime_error const& e) {
        LOG(log_.fatal()) << "Failed to load " << data.seq << ": " << e.what();
        amendmentBlockHandler_->notifyAmendmentBlocked();
    }
};

void
Loader::onInitialLoadGotMoreObjects(
    uint32_t seq,
    std::vector<model::Object> const& data,
    std::optional<std::string> lastKey
)
{
    LOG(log_.debug()) << "On initial load: got more objects for seq " << seq << ". size = " << data.size();
    registry_->dispatchInitialObjects(
        seq, data, std::move(lastKey).value_or(std::string{})  // TODO: perhaps use optional all the way to extensions?
    );
}

std::optional<ripple::LedgerHeader>
Loader::loadInitialLedger(model::LedgerData const& data)
{
    // check that database is actually empty
    auto rng = backend_->hardFetchLedgerRangeNoThrow();
    if (rng) {
        ASSERT(false, "Database is not empty");
        return std::nullopt;
    }

    LOG(log_.debug()) << "Deserialized ledger header. " << ::util::toString(data.header);

    auto seconds = ::util::timed<std::chrono::seconds>([this, &data]() { registry_->dispatchInitialData(data); });
    LOG(log_.info()) << "Dispatching initial data and submitting all writes took " << seconds << " seconds.";

    backend_->finishWrites(data.seq);
    LOG(log_.debug()) << "Loaded initial ledger";

    return {data.header};
}

}  // namespace etlng::impl
