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
#include "etlng/Models.hpp"
#include "etlng/SchedulerInterface.hpp"

#include <sys/types.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace etlng::impl {

template <typename T>
concept SomeScheduler = std::is_base_of_v<SchedulerInterface, std::decay_t<T>>;

class ForwardScheduler : public SchedulerInterface {
    std::reference_wrapper<etl::NetworkValidatedLedgersInterface> ledgers_;

    uint32_t startSeq_;
    std::optional<uint32_t> maxSeq_;
    std::atomic_uint32_t seq_;

public:
    ForwardScheduler(ForwardScheduler const& other)
        : ledgers_(other.ledgers_), startSeq_(other.startSeq_), maxSeq_(other.maxSeq_), seq_(other.seq_.load())
    {
    }

    ForwardScheduler(
        std::reference_wrapper<etl::NetworkValidatedLedgersInterface> ledgers,
        uint32_t startSeq,
        std::optional<uint32_t> maxSeq = std::nullopt
    )
        : ledgers_(ledgers), startSeq_(startSeq), maxSeq_(maxSeq), seq_(startSeq)
    {
    }

    [[nodiscard]] std::optional<model::Task>
    next() override
    {
        static constexpr auto kMAX = std::numeric_limits<uint32_t>::max();
        uint32_t currentSeq = seq_;

        if (ledgers_.get().getMostRecent() >= currentSeq) {
            while (currentSeq < maxSeq_.value_or(kMAX)) {
                if (seq_.compare_exchange_weak(currentSeq, currentSeq + 1u, std::memory_order_acq_rel)) {
                    return {{.priority = model::Task::Priority::Higher, .seq = currentSeq}};
                }
            }
        }

        return std::nullopt;
    }
};

class BackfillScheduler : public SchedulerInterface {
    uint32_t startSeq_;
    uint32_t minSeq_ = 0u;

    std::atomic_uint32_t seq_;

public:
    BackfillScheduler(BackfillScheduler const& other)
        : startSeq_(other.startSeq_), minSeq_(other.minSeq_), seq_(other.seq_.load())
    {
    }

    BackfillScheduler(uint32_t startSeq, std::optional<uint32_t> minSeq = std::nullopt)
        : startSeq_(startSeq), minSeq_(minSeq.value_or(0)), seq_(startSeq)
    {
    }

    [[nodiscard]] std::optional<model::Task>
    next() override
    {
        uint32_t currentSeq = seq_;
        while (currentSeq > minSeq_) {
            if (seq_.compare_exchange_weak(currentSeq, currentSeq - 1u, std::memory_order_acq_rel)) {
                return {{.priority = model::Task::Priority::Lower, .seq = currentSeq}};
            }
        }

        return std::nullopt;
    }
};

template <SomeScheduler... Schedulers>
class SchedulerChain : public SchedulerInterface {
    std::tuple<Schedulers...> schedulers_;

public:
    template <SomeScheduler... Ts>
        requires(std::is_same_v<Ts, Schedulers> and ...)
    SchedulerChain(Ts&&... schedulers) : schedulers_(std::forward<Ts>(schedulers)...)
    {
    }

    [[nodiscard]] std::optional<model::Task>
    next() override
    {
        std::optional<model::Task> task;
        auto const expand = [&](auto& s) {
            if (task.has_value())
                return false;

            task = s.next();
            return task.has_value();
        };

        std::apply([&expand](auto&&... xs) { (... || expand(xs)); }, schedulers_);

        return task;
    }
};

static auto
makeScheduler(SomeScheduler auto&&... schedulers)
{
    return std::make_unique<SchedulerChain<std::decay_t<decltype(schedulers)>...>>(
        std::forward<decltype(schedulers)>(schedulers)...
    );
}

}  // namespace etlng::impl
