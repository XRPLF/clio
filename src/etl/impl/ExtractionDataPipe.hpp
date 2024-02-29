//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include "etl/ETLHelpers.hpp"
#include "util/log/Logger.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace etl::impl {

/**
 * @brief A collection of thread safe async queues used by Extractor and Transformer to communicate
 */
template <typename RawDataType>
class ExtractionDataPipe {
public:
    using DataType = std::optional<RawDataType>;
    using QueueType = ThreadSafeQueue<DataType>;  // TODO: probably should use boost::lockfree::queue instead?

    constexpr static auto TOTAL_MAX_IN_QUEUE = 1000u;

private:
    util::Logger log_{"ETL"};

    uint32_t stride_;
    uint32_t startSequence_;

    std::vector<std::shared_ptr<QueueType>> queues_;

public:
    /**
     * @brief Create a new instance of the extraction data pipe
     *
     * @param stride
     * @param startSequence
     */
    ExtractionDataPipe(uint32_t stride, uint32_t startSequence) : stride_{stride}, startSequence_{startSequence}
    {
        auto const maxQueueSize = TOTAL_MAX_IN_QUEUE / stride;
        for (size_t i = 0; i < stride_; ++i)
            queues_.push_back(std::make_unique<QueueType>(maxQueueSize));
    }

    /**
     * @brief Push new data package for the specified sequence.
     *
     * Note: Potentially blocks until the underlying queue can accomodate another entry.
     *
     * @param sequence The sequence for which to enqueue the data package
     * @param data The data to store
     */
    void
    push(uint32_t sequence, DataType&& data)
    {
        getQueue(sequence)->push(std::move(data));
    }

    /**
     * @brief Get data package for the given sequence
     *
     * Note: Potentially blocks until data is available.
     *
     * @param sequence The sequence for which data is required
     * @return The data wrapped in an optional; nullopt means that there is no more data to expect
     */
    DataType
    popNext(uint32_t sequence)
    {
        return getQueue(sequence)->pop();
    }

    /**
     * @return Get the stride
     */
    uint32_t
    getStride() const
    {
        return stride_;
    }

    /**
     * @brief Hint the Transformer that the queue is done sending data
     * @param sequence The sequence for which the extractor queue is to be hinted
     */
    void
    finish(uint32_t sequence)
    {
        // empty optional hints the Transformer to shut down
        push(sequence, std::nullopt);
    }

    /**
     * @brief Unblocks internal queues
     *
     * Note: For now this must be called by the ETL when Transformer exits.
     */
    void
    cleanup()
    {
        // TODO: this should not have to be called by hand. it should be done via RAII
        for (auto i = 0u; i < stride_; ++i)
            getQueue(i)->tryPop();  // pop from each queue that might be blocked on a push
    }

private:
    std::shared_ptr<QueueType>
    getQueue(uint32_t sequence)
    {
        LOG(log_.debug()) << "Grabbing extraction queue for " << sequence << "; start was " << startSequence_;
        return queues_[(sequence - startSequence_) % stride_];
    }
};

}  // namespace etl::impl
