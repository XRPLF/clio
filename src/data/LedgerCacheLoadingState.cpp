#include "data/LedgerCacheLoadingState.hpp"

#include "data/LedgerCacheInterface.hpp"

#include <memory>

namespace data {

LedgerCacheLoadingState::LedgerCacheLoadingState(LedgerCacheInterface const& cache) : cache_(cache)
{
}

void
LedgerCacheLoadingState::allowLoading()
{
    *isLoadingAllowed_ = true;
    isLoadingAllowed_->notify_all();
}

bool
LedgerCacheLoadingState::isLoadingAllowed() const
{
    return *isLoadingAllowed_;
}

void
LedgerCacheLoadingState::waitForLoadingAllowed() const
{
    isLoadingAllowed_->wait(false);
}

bool
LedgerCacheLoadingState::isCurrentlyLoading() const
{
    return cache_.get().isCurrentlyLoading();
}

std::unique_ptr<LedgerCacheLoadingStateInterface>
LedgerCacheLoadingState::clone() const
{
    return std::make_unique<LedgerCacheLoadingState>(*this);
}

}  // namespace data
