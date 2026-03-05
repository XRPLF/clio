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
}

bool
WriterState::isFallback() const
{
    return systemState_->isWriterDecidingFallback;
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
