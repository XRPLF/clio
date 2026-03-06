//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2026, the clio developers.

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

#include "data/LedgerCacheInterface.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <utility>
namespace data {

class LedgerCacheLoadingStateInterface {
public:
    virtual ~LedgerCacheLoadingStateInterface() = default;

    virtual void
    allowLoading() = 0;

    [[nodiscard]] virtual bool
    isLoadingAllowed() const = 0;

    virtual void
    waitForLoadingAllowed() const = 0;

    [[nodiscard]] virtual bool
    isCurrentlyLoading() const = 0;

    [[nodiscard]] virtual std::unique_ptr<LedgerCacheLoadingStateInterface>
    clone() const = 0;
};

class LedgerCacheLoadingState : public LedgerCacheLoadingStateInterface {
    std::reference_wrapper<LedgerCacheInterface const> cache_;
    std::shared_ptr<std::atomic_bool> isLoadingAllowed_ = std::make_shared<std::atomic_bool>(false);

public:
    explicit LedgerCacheLoadingState(LedgerCacheInterface const& cache) : cache_(cache)
    {
    }

    void
    allowLoading() override
    {
        *isLoadingAllowed_ = true;
    }

    [[nodiscard]] bool
    isLoadingAllowed() const override
    {
        return *isLoadingAllowed_;
    }

    void
    waitForLoadingAllowed() const override
    {
        isLoadingAllowed_->wait(false);
    }

    bool
    isCurrentlyLoading() const override
    {
        return cache_.get().isCurrentlyLoading();
    }

    [[nodiscard]] std::unique_ptr<LedgerCacheLoadingStateInterface>
    clone() const override
    {
        auto result = *this;
        return std::make_unique<LedgerCacheLoadingState>(std::move(result));
    }
};

}  // namespace data
