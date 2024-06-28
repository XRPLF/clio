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

#include "etl/SystemState.hpp"
#include "etl/impl/Extractor.hpp"
#include "util/FakeFetchResponse.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockExtractionDataPipe.hpp"
#include "util/MockLedgerFetcher.hpp"
#include "util/MockNetworkValidatedLedgers.hpp"
#include "util/MockPrometheus.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>

using namespace testing;
using namespace etl;

struct ETLExtractorTest : util::prometheus::WithPrometheus, NoLoggerFixture {
    using ExtractionDataPipeType = MockExtractionDataPipe;
    using LedgerFetcherType = MockLedgerFetcher;
    using ExtractorType = etl::impl::Extractor<ExtractionDataPipeType, LedgerFetcherType>;

    ExtractionDataPipeType dataPipe_;
    MockNetworkValidatedLedgersPtr networkValidatedLedgers_;
    LedgerFetcherType ledgerFetcher_;
    SystemState state_;

    ETLExtractorTest()
    {
        state_.isStopping = false;
        state_.writeConflict = false;
        state_.isReadOnly = false;
        state_.isWriting = false;
    }
};

TEST_F(ETLExtractorTest, StopsWhenCurrentSequenceExceedsFinishSequence)
{
    EXPECT_CALL(*networkValidatedLedgers_, waitUntilValidatedByNetwork).Times(3).WillRepeatedly(Return(true));
    EXPECT_CALL(dataPipe_, getStride).Times(3).WillRepeatedly(Return(4));

    auto response = FakeFetchResponse{};
    EXPECT_CALL(ledgerFetcher_, fetchDataAndDiff).Times(3).WillRepeatedly(Return(response));
    EXPECT_CALL(dataPipe_, push).Times(3);
    EXPECT_CALL(dataPipe_, finish(0)).Times(1);

    // expected to invoke for seq 0, 4, 8 and finally stop as seq will be greater than finishing seq
    ExtractorType{dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 0, 11, state_};
}

TEST_F(ETLExtractorTest, StopsOnWriteConflict)
{
    EXPECT_CALL(dataPipe_, finish(0));
    state_.writeConflict = true;

    // despite finish sequence being far ahead, we set writeConflict and so exit the loop immediately
    ExtractorType{dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 0, 64, state_};
}

TEST_F(ETLExtractorTest, StopsOnServerShutdown)
{
    EXPECT_CALL(dataPipe_, finish(0));
    state_.isStopping = true;

    // despite finish sequence being far ahead, we set isStopping and so exit the loop immediately
    ExtractorType{dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 0, 64, state_};
}

// stop extractor thread if fetcheResponse is empty
TEST_F(ETLExtractorTest, StopsIfFetchIsUnsuccessful)
{
    EXPECT_CALL(*networkValidatedLedgers_, waitUntilValidatedByNetwork).WillOnce(Return(true));

    EXPECT_CALL(ledgerFetcher_, fetchDataAndDiff).WillOnce(Return(std::nullopt));
    EXPECT_CALL(dataPipe_, finish(0));

    // we break immediately because fetchDataAndDiff returns nullopt
    ExtractorType{dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 0, 64, state_};
}

TEST_F(ETLExtractorTest, StopsIfWaitingUntilValidatedByNetworkTimesOut)
{
    // note that in actual clio code we don't return false unless a timeout is specified and exceeded
    EXPECT_CALL(*networkValidatedLedgers_, waitUntilValidatedByNetwork).WillOnce(Return(false));
    EXPECT_CALL(dataPipe_, finish(0)).Times(1);

    // we emulate waitUntilValidatedByNetwork timing out which would lead to shutdown of the extractor thread
    ExtractorType{dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 0, 64, state_};
}

TEST_F(ETLExtractorTest, SendsCorrectResponseToDataPipe)
{
    EXPECT_CALL(*networkValidatedLedgers_, waitUntilValidatedByNetwork).WillOnce(Return(true));
    EXPECT_CALL(dataPipe_, getStride).WillOnce(Return(4));

    auto response = FakeFetchResponse{1234};

    EXPECT_CALL(ledgerFetcher_, fetchDataAndDiff).WillOnce(Return(response));
    EXPECT_CALL(dataPipe_, push(_, std::optional{response}));
    EXPECT_CALL(dataPipe_, finish(0));

    // expect to finish after just one response due to finishSequence set to 1
    ExtractorType extractor{dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 0, 1, state_};
    extractor.waitTillFinished();  // this is what clio does too. waiting for the thread to join
}

TEST_F(ETLExtractorTest, CallsPipeFinishWithInitialSequenceAtExit)
{
    EXPECT_CALL(dataPipe_, finish(123));
    state_.isStopping = true;

    ExtractorType{dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 123, 234, state_};
}
