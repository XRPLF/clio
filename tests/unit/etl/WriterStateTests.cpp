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

#include "etl/SystemState.hpp"
#include "etl/WriterState.hpp"
#include "util/MockLedgerCache.hpp"
#include "util/MockPrometheus.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

using namespace etl;
using namespace testing;

struct WriterStateTest : util::prometheus::WithPrometheus {
    std::shared_ptr<SystemState> systemState = std::make_shared<SystemState>();
    StrictMock<MockFunction<void(SystemState::WriteCommand)>> mockWriteCommand;
    NiceMock<MockLedgerCache> cache;
    WriterState writerState{systemState, cache};

    WriterStateTest()
    {
        systemState->writeCommandSignal.connect(mockWriteCommand.AsStdFunction());
    }
};

TEST_F(WriterStateTest, IsWritingReturnsSystemStateValue)
{
    systemState->isWriting = false;
    EXPECT_FALSE(writerState.isWriting());

    systemState->isWriting = true;
    EXPECT_TRUE(writerState.isWriting());
}

TEST_F(WriterStateTest, StartWritingEmitsStartWritingCommand)
{
    systemState->isWriting = false;

    EXPECT_CALL(mockWriteCommand, Call(SystemState::WriteCommand::StartWriting));

    writerState.startWriting();
}

TEST_F(WriterStateTest, StartWritingDoesNothingWhenAlreadyWriting)
{
    systemState->isWriting = true;

    // No EXPECT_CALL - StrictMock will fail if any command is emitted

    writerState.startWriting();
}

TEST_F(WriterStateTest, GiveUpWritingEmitsStopWritingCommand)
{
    systemState->isWriting = true;

    EXPECT_CALL(mockWriteCommand, Call(SystemState::WriteCommand::StopWriting));

    writerState.giveUpWriting();
}

TEST_F(WriterStateTest, GiveUpWritingDoesNothingWhenNotWriting)
{
    systemState->isWriting = false;

    // No EXPECT_CALL - StrictMock will fail if any command is emitted

    writerState.giveUpWriting();
}

TEST_F(WriterStateTest, IsFallbackReturnsFalseByDefault)
{
    EXPECT_FALSE(writerState.isFallback());
}

TEST_F(WriterStateTest, SetWriterDecidingFallbackSetsFlag)
{
    EXPECT_FALSE(systemState->isWriterDecidingFallback);

    writerState.setWriterDecidingFallback();

    EXPECT_TRUE(systemState->isWriterDecidingFallback);
}

TEST_F(WriterStateTest, IsFallbackReturnsSystemStateValue)
{
    systemState->isWriterDecidingFallback = false;
    EXPECT_FALSE(writerState.isFallback());

    systemState->isWriterDecidingFallback = true;
    EXPECT_TRUE(writerState.isFallback());
}

TEST_F(WriterStateTest, IsReadOnlyReturnsSystemStateValue)
{
    systemState->isStrictReadonly = false;
    EXPECT_FALSE(writerState.isReadOnly());

    systemState->isStrictReadonly = true;
    EXPECT_TRUE(writerState.isReadOnly());
}

TEST_F(WriterStateTest, IsEtlStartedReturnsSystemStateValue)
{
    systemState->etlStarted = false;
    EXPECT_FALSE(writerState.isEtlStarted());

    systemState->etlStarted = true;
    EXPECT_TRUE(writerState.isEtlStarted());
}

TEST_F(WriterStateTest, IsCacheFullReturnsCacheValue)
{
    EXPECT_CALL(cache, isFull()).WillOnce(Return(false));
    EXPECT_FALSE(writerState.isCacheFull());

    EXPECT_CALL(cache, isFull()).WillOnce(Return(true));
    EXPECT_TRUE(writerState.isCacheFull());
}

TEST_F(WriterStateTest, CloneCreatesNewInstanceWithSameSystemState)
{
    systemState->isWriting = true;
    systemState->isStrictReadonly = true;
    systemState->etlStarted = false;

    auto cloned = writerState.clone();

    ASSERT_NE(cloned.get(), &writerState);
    EXPECT_TRUE(cloned->isWriting());
    EXPECT_TRUE(cloned->isReadOnly());
    EXPECT_FALSE(cloned->isEtlStarted());
}

TEST_F(WriterStateTest, ClonedInstanceSharesSystemState)
{
    auto cloned = writerState.clone();

    systemState->isWriting = true;

    EXPECT_TRUE(writerState.isWriting());
    EXPECT_TRUE(cloned->isWriting());

    systemState->isWriting = false;

    EXPECT_FALSE(writerState.isWriting());
    EXPECT_FALSE(cloned->isWriting());

    EXPECT_FALSE(writerState.isFallback());
    EXPECT_FALSE(cloned->isFallback());
    cloned->setWriterDecidingFallback();
    EXPECT_TRUE(writerState.isFallback());
    EXPECT_TRUE(cloned->isFallback());
}

TEST_F(WriterStateTest, IsFallbackRecoveryReturnsFalseByDefault)
{
    EXPECT_FALSE(writerState.isFallbackRecovery());
}

TEST_F(WriterStateTest, SetFallbackRecoveryTrueSetsFlag)
{
    writerState.setFallbackRecovery(true);
    EXPECT_TRUE(writerState.isFallbackRecovery());
}

TEST_F(WriterStateTest, SetFallbackRecoveryTrueClearsFallbackFlag)
{
    systemState->isWriterDecidingFallback = true;
    EXPECT_TRUE(writerState.isFallback());

    writerState.setFallbackRecovery(true);

    EXPECT_FALSE(writerState.isFallback());
    EXPECT_TRUE(writerState.isFallbackRecovery());
}

TEST_F(WriterStateTest, SetFallbackRecoveryFalseClearsFlag)
{
    writerState.setFallbackRecovery(true);
    ASSERT_TRUE(writerState.isFallbackRecovery());

    writerState.setFallbackRecovery(false);
    EXPECT_FALSE(writerState.isFallbackRecovery());
}

TEST_F(WriterStateTest, SetFallbackRecoveryFalseDoesNotAffectFallbackFlag)
{
    systemState->isWriterDecidingFallback = true;

    writerState.setFallbackRecovery(false);

    EXPECT_TRUE(writerState.isFallback());
}

TEST_F(WriterStateTest, SetWriterDecidingFallbackClearsFallbackRecovery)
{
    writerState.setFallbackRecovery(true);
    ASSERT_TRUE(writerState.isFallbackRecovery());

    writerState.setWriterDecidingFallback();

    EXPECT_FALSE(writerState.isFallbackRecovery());
    EXPECT_TRUE(writerState.isFallback());
}

TEST_F(WriterStateTest, ClonedInstanceSharesFallbackRecovery)
{
    // prometheus::Bool holds a reference_wrapper to the underlying gauge,
    // so clone and original share the same metric value.
    auto cloned = writerState.clone();

    EXPECT_FALSE(writerState.isFallbackRecovery());
    EXPECT_FALSE(cloned->isFallbackRecovery());

    systemState->isWriterDecidingFallback = true;  // precondition for setFallbackRecovery(true)
    cloned->setFallbackRecovery(true);

    EXPECT_TRUE(writerState.isFallbackRecovery());
    EXPECT_TRUE(cloned->isFallbackRecovery());
    // setFallbackRecovery(true) also clears the fallback flag on shared SystemState
    EXPECT_FALSE(writerState.isFallback());
}
