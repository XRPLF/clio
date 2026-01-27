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
#include "util/MockPrometheus.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

using namespace etl;
using namespace testing;

struct WriterStateTest : util::prometheus::WithPrometheus {
    std::shared_ptr<SystemState> systemState = std::make_shared<SystemState>();
    StrictMock<MockFunction<void(SystemState::WriteCommand)>> mockWriteCommand;
    WriterState writerState{systemState};

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

TEST_F(WriterStateTest, IsLoadingCacheReturnsSystemStateValue)
{
    systemState->isLoadingCache = false;
    EXPECT_FALSE(writerState.isLoadingCache());

    systemState->isLoadingCache = true;
    EXPECT_TRUE(writerState.isLoadingCache());
}

TEST_F(WriterStateTest, CloneCreatesNewInstanceWithSameSystemState)
{
    systemState->isWriting = true;
    systemState->isStrictReadonly = true;
    systemState->isLoadingCache = false;

    auto cloned = writerState.clone();

    ASSERT_NE(cloned.get(), &writerState);
    EXPECT_TRUE(cloned->isWriting());
    EXPECT_TRUE(cloned->isReadOnly());
    EXPECT_FALSE(cloned->isLoadingCache());
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
