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

#include <log/Logger.h>

#include <ripple/ledger/ReadView.h>
#include "org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h"
#include <grpcpp/grpcpp.h>

#include <atomic>
#include <string>
#include <vector>

namespace clio::detail {

/**
 * @brief A collection of thread safe async queues used by Extractor and Transformer to communicate
 */
class ExtractionDataPipe
{
    clio::Logger log_{"ETL"};

    uint32_t numQueues_;
    uint32_t startSequence_;

    using DataType = std::optional<org::xrpl::rpc::v1::GetLedgerResponse>;
    using QueueType = ThreadSafeQueue<DataType>;  // TODO: probably should use boost::lockfree::queue instead?

    std::vector<std::shared_ptr<QueueType>> queues_;

public:
    ExtractionDataPipe(uint32_t numQueues, uint32_t startSequence)
        : numQueues_{numQueues}, startSequence_{startSequence}
    {
        uint32_t const maxQueueSize = 1000u / numQueues_;
        for (size_t i = 0; i < numQueues_; ++i)
            queues_.push_back(std::make_shared<QueueType>(maxQueueSize));
    }

    std::shared_ptr<QueueType>
    getExtractor(uint32_t sequence)
    {
        log_.debug() << "Grabbing extractor for " << sequence << "; start was " << startSequence_;
        return queues_[(sequence - startSequence_) % numQueues_];
    }

    DataType
    popNext(uint32_t sequence)
    {
        return getExtractor(sequence)->pop();
    }

    uint32_t
    numQueues() const
    {
        return numQueues_;
    }

    void
    cleanup()
    {
        for (auto i = 0u; i < numQueues_; ++i)
            getExtractor(i)->tryPop();  // pop from each queue that might be blocked on a push
    }
};

}  // namespace clio::detail
