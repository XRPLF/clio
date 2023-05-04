//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include <boost/json.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <rpc/WorkQueue.h>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace RPC {

class Counters
{
private:
    struct MethodInfo
    {
        MethodInfo() = default;

        std::atomic_uint64_t started{0};
        std::atomic_uint64_t finished{0};
        std::atomic_uint64_t errored{0};
        std::atomic_uint64_t forwarded{0};
        std::atomic_uint64_t duration{0};
    };

    void
    initializeCounter(std::string const& method);

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, MethodInfo> methodInfo_;

    std::reference_wrapper<const WorkQueue> workQueue_;

public:
    Counters(WorkQueue const& wq) : workQueue_(std::cref(wq)){};

    static Counters
    make_Counters(WorkQueue const& wq)
    {
        return Counters{wq};
    }

    void
    rpcErrored(std::string const& method);

    void
    rpcComplete(std::string const& method, std::chrono::microseconds const& rpcDuration);

    void
    rpcForwarded(std::string const& method);

    boost::json::object
    report() const;
};

}  // namespace RPC
