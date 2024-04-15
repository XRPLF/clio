//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "util/Assert.hpp"

#include <atomic>
#include <cstddef>

namespace util {

/**
 * @brief Class limiting the number of instances of itself.
 *
 * @tparam MaxInstances The maximum number of instances allowed.
 */
template <size_t MaxInstances = 1>
class InstancesLimiter {
    static std::atomic<size_t> instancesCount_;

public:
    /**
     * @brief Constructor incrementing the number of instances.
     */
    InstancesLimiter()
    {
        ++instancesCount_;
        ASSERT(
            instancesCount_ <= MaxInstances,
            "Too many instances {} while allowed {}.",
            instancesCount_.load(),
            MaxInstances
        );
    }

    /**
     * @brief Destructor decrementing the number of instances.
     */
    virtual ~InstancesLimiter()
    {
        ASSERT(instancesCount_ > 0, "Deleting an instance twice");
        --instancesCount_;
    }
};

template <size_t MaxInstances>
std::atomic<size_t> InstancesLimiter<MaxInstances>::instancesCount_ = 0;

}  // namespace util
