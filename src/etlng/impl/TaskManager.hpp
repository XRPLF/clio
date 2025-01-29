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

#include "etlng/ExtractorInterface.hpp"
#include "etlng/LoaderInterface.hpp"
#include "etlng/Models.hpp"
#include "etlng/SchedulerInterface.hpp"
#include "util/StrandedPriorityQueue.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/async/AnyStrand.hpp"
#include "util/log/Logger.hpp"

#include <xrpl/protocol/TxFormats.h>

#include <cstddef>
#include <functional>
#include <vector>

namespace etlng::impl {

class TaskManager {
    util::async::AnyExecutionContext ctx_;
    std::reference_wrapper<SchedulerInterface> schedulers_;
    std::reference_wrapper<ExtractorInterface> extractor_;
    std::reference_wrapper<LoaderInterface> loader_;

    std::vector<util::async::AnyOperation<void>> extractors_;
    std::vector<util::async::AnyOperation<void>> loaders_;

    util::Logger log_{"ETL"};

    struct ReverseOrderComparator {
        [[nodiscard]] bool
        operator()(model::LedgerData const& lhs, model::LedgerData const& rhs) const noexcept
        {
            return lhs.seq > rhs.seq;
        }
    };

public:
    struct Settings {
        size_t numExtractors; /**< number of extraction tasks */
        size_t numLoaders;    /**< number of loading tasks */
    };

    // reverse order loading is needed (i.e. start with oldest seq in forward fill buffer)
    using PriorityQueue = util::StrandedPriorityQueue<model::LedgerData, ReverseOrderComparator>;

    TaskManager(
        util::async::AnyExecutionContext&& ctx,
        std::reference_wrapper<SchedulerInterface> scheduler,
        std::reference_wrapper<ExtractorInterface> extractor,
        std::reference_wrapper<LoaderInterface> loader
    );

    ~TaskManager();

    void
    run(Settings settings);

    void
    stop();

private:
    void
    wait();

    [[nodiscard]] util::async::AnyOperation<void>
    spawnExtractor(util::async::AnyStrand& strand, PriorityQueue& queue);

    [[nodiscard]] util::async::AnyOperation<void>
    spawnLoader(PriorityQueue& queue);
};

}  // namespace etlng::impl
