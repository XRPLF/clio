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

#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etlng/ExtractorInterface.hpp"
#include "etlng/LoaderInterface.hpp"
#include "etlng/MonitorInterface.hpp"
#include "etlng/TaskManagerInterface.hpp"
#include "etlng/TaskManagerProviderInterface.hpp"
#include "etlng/impl/Scheduling.hpp"
#include "etlng/impl/TaskManager.hpp"
#include "util/async/AnyExecutionContext.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace etlng::impl {

/**
 * @brief Implementation of the TaskManagerProvider interface
 */
class TaskManagerProvider : public TaskManagerProviderInterface {
    std::reference_wrapper<etl::NetworkValidatedLedgersInterface> ledgers_;
    std::shared_ptr<ExtractorInterface> extractor_;
    std::shared_ptr<LoaderInterface> loader_;

public:
    /**
     * @brief Constructor
     *
     * @param ledgers Reference to ledgers
     * @param extractor The extractor
     * @param loader The loader
     */
    TaskManagerProvider(
        std::reference_wrapper<etl::NetworkValidatedLedgersInterface> ledgers,
        std::shared_ptr<ExtractorInterface> extractor,
        std::shared_ptr<LoaderInterface> loader
    )
        : ledgers_(ledgers), extractor_(std::move(extractor)), loader_(std::move(loader))
    {
    }

    std::unique_ptr<TaskManagerInterface>
    make(
        util::async::AnyExecutionContext ctx,
        std::reference_wrapper<MonitorInterface> monitor,
        uint32_t startSeq,
        std::optional<uint32_t> finishSeq
    ) override
    {
        auto scheduler = impl::makeScheduler(impl::ForwardScheduler{ledgers_, startSeq, finishSeq});
        // TODO: add impl::BackfillScheduler{startSeq - 1, startSeq - ...},

        return std::make_unique<TaskManager>(
            std::move(ctx), std::move(scheduler), *extractor_, *loader_, monitor, startSeq
        );
    }
};

}  // namespace etlng::impl
