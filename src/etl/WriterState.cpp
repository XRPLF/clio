#include "etl/WriterState.hpp"

#include "data/LedgerCacheInterface.hpp"
#include "etl/SystemState.hpp"

#include <memory>
#include <utility>

namespace etl {

WriterState::WriterState(
    std::shared_ptr<SystemState> state,
    data::LedgerCacheInterface const& cache
)
    : systemState_(std::move(state)), cache_(cache)
{
}

bool
WriterState::isReadOnly() const
{
    return systemState_->isStrictReadonly;
}

bool
WriterState::isWriting() const
{
    return systemState_->isWriting;
}

void
WriterState::startWriting()
{
    if (isWriting())
        return;

    systemState_->writeCommandSignal(SystemState::WriteCommand::StartWriting);
}

void
WriterState::giveUpWriting()
{
    if (not isWriting())
        return;

    systemState_->writeCommandSignal(SystemState::WriteCommand::StopWriting);
}

void
WriterState::setWriterDecidingFallback()
{
    systemState_->isWriterDecidingFallback = true;
    isFallbackRecovery_ = false;
}

bool
WriterState::isFallback() const
{
    return systemState_->isWriterDecidingFallback;
}

bool
WriterState::isFallbackRecovery() const
{
    return isFallbackRecovery_;
}

void
WriterState::setFallbackRecovery(bool newValue)
{
    if (newValue) {
        systemState_->isWriterDecidingFallback = false;
    }
    isFallbackRecovery_ = newValue;
}

bool
WriterState::isEtlStarted() const
{
    return systemState_->etlStarted;
}

bool
WriterState::isCacheFull() const
{
    return cache_.get().isFull();
}

std::unique_ptr<WriterStateInterface>
WriterState::clone() const
{
    auto c = WriterState(*this);
    return std::make_unique<WriterState>(std::move(c));
}

}  // namespace etl
