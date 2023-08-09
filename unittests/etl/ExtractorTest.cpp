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

#include <etl/impl/Extractor.h>
#include <util/FakeFetchResponse.h>
#include <util/Fixtures.h>
#include <util/MockExtractionDataPipe.h>
#include <util/MockLedgerFetcher.h>
#include <util/MockNetworkValidatedLedgers.h>

#include <gtest/gtest.h>

#include <memory>

using namespace testing;

class ETLExtractorTest : public NoLoggerFixture
{
protected:
    using ExtractionDataPipeType = MockExtractionDataPipe;
    using LedgerFetcherType = MockLedgerFetcher;
    using ExtractorType =
        clio::etl::detail::Extractor<ExtractionDataPipeType, MockNetworkValidatedLedgers, LedgerFetcherType>;

    ExtractionDataPipeType dataPipe_;
    std::shared_ptr<MockNetworkValidatedLedgers> networkValidatedLedgers_ =
        std::make_shared<MockNetworkValidatedLedgers>();
    LedgerFetcherType ledgerFetcher_;
    SystemState state_;

    std::unique_ptr<ExtractorType> extractor_;

public:
    void
    SetUp() override
    {
        NoLoggerFixture::SetUp();
        state_.isStopping = false;
        state_.writeConflict = false;
        state_.isReadOnly = false;
        state_.isWriting = false;
    }

    void
    TearDown() override
    {
        extractor_.reset();
        NoLoggerFixture::TearDown();
    }
};

TEST_F(ETLExtractorTest, StopsWhenCurrentSequenceExceedsFinishSequence)
{
    auto const rawNetworkValidatedLedgersPtr =
        static_cast<MockNetworkValidatedLedgers*>(networkValidatedLedgers_.get());

    ON_CALL(*rawNetworkValidatedLedgersPtr, waitUntilValidatedByNetwork).WillByDefault(Return(true));
    EXPECT_CALL(*rawNetworkValidatedLedgersPtr, waitUntilValidatedByNetwork).Times(3);
    ON_CALL(dataPipe_, getStride).WillByDefault(Return(4));
    EXPECT_CALL(dataPipe_, getStride).Times(3);

    auto response = FakeFetchResponse{};
    ON_CALL(ledgerFetcher_, fetchDataAndDiff(_)).WillByDefault(Return(response));
    EXPECT_CALL(ledgerFetcher_, fetchDataAndDiff).Times(3);
    EXPECT_CALL(dataPipe_, push).Times(3);
    EXPECT_CALL(dataPipe_, finish(0)).Times(1);

    // expected to invoke for seq 0, 4, 8 and finally stop as seq will be greater than finishing seq
    extractor_ = std::make_unique<ExtractorType>(dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 0, 11, state_);
}

TEST_F(ETLExtractorTest, StopsOnWriteConflict)
{
    EXPECT_CALL(dataPipe_, finish(0)).Times(1);
    state_.writeConflict = true;

    // despite finish sequence being far ahead, we set writeConflict and so exit the loop immediately
    extractor_ = std::make_unique<ExtractorType>(dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 0, 64, state_);
}

TEST_F(ETLExtractorTest, StopsOnServerShutdown)
{
    EXPECT_CALL(dataPipe_, finish(0)).Times(1);
    state_.isStopping = true;

    // despite finish sequence being far ahead, we set isStopping and so exit the loop immediately
    extractor_ = std::make_unique<ExtractorType>(dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 0, 64, state_);
}

// stop extractor thread if fetcheResponse is empty
TEST_F(ETLExtractorTest, StopsIfFetchIsUnsuccessful)
{
    auto const rawNetworkValidatedLedgersPtr =
        static_cast<MockNetworkValidatedLedgers*>(networkValidatedLedgers_.get());

    ON_CALL(*rawNetworkValidatedLedgersPtr, waitUntilValidatedByNetwork).WillByDefault(Return(true));
    EXPECT_CALL(*rawNetworkValidatedLedgersPtr, waitUntilValidatedByNetwork).Times(1);

    ON_CALL(ledgerFetcher_, fetchDataAndDiff(_)).WillByDefault(Return(std::nullopt));
    EXPECT_CALL(ledgerFetcher_, fetchDataAndDiff).Times(1);
    EXPECT_CALL(dataPipe_, finish(0)).Times(1);

    // we break immediately because fetchDataAndDiff returns nullopt
    extractor_ = std::make_unique<ExtractorType>(dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 0, 64, state_);
}

TEST_F(ETLExtractorTest, StopsIfWaitingUntilValidatedByNetworkTimesOut)
{
    auto const rawNetworkValidatedLedgersPtr =
        static_cast<MockNetworkValidatedLedgers*>(networkValidatedLedgers_.get());

    // note that in actual clio code we don't return false unless a timeout is specified and exceeded
    ON_CALL(*rawNetworkValidatedLedgersPtr, waitUntilValidatedByNetwork).WillByDefault(Return(false));
    EXPECT_CALL(*rawNetworkValidatedLedgersPtr, waitUntilValidatedByNetwork).Times(1);
    EXPECT_CALL(dataPipe_, finish(0)).Times(1);

    // we emulate waitUntilValidatedByNetwork timing out which would lead to shutdown of the extractor thread
    extractor_ = std::make_unique<ExtractorType>(dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 0, 64, state_);
}

TEST_F(ETLExtractorTest, SendsCorrectResponseToDataPipe)
{
    auto const rawNetworkValidatedLedgersPtr =
        static_cast<MockNetworkValidatedLedgers*>(networkValidatedLedgers_.get());

    ON_CALL(*rawNetworkValidatedLedgersPtr, waitUntilValidatedByNetwork).WillByDefault(Return(true));
    EXPECT_CALL(*rawNetworkValidatedLedgersPtr, waitUntilValidatedByNetwork).Times(1);
    ON_CALL(dataPipe_, getStride).WillByDefault(Return(4));
    EXPECT_CALL(dataPipe_, getStride).Times(1);

    auto response = FakeFetchResponse{1234};
    auto optionalResponse = std::optional<FakeFetchResponse>{};

    ON_CALL(ledgerFetcher_, fetchDataAndDiff(_)).WillByDefault(Return(response));
    EXPECT_CALL(ledgerFetcher_, fetchDataAndDiff).Times(1);
    EXPECT_CALL(dataPipe_, push).Times(1).WillOnce(SaveArg<1>(&optionalResponse));
    EXPECT_CALL(dataPipe_, finish(0)).Times(1);

    // expect to finish after just one response due to finishSequence set to 1
    extractor_ = std::make_unique<ExtractorType>(dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 0, 1, state_);
    extractor_->waitTillFinished();  // this is what clio does too. waiting for the thread to join

    EXPECT_TRUE(optionalResponse.has_value());
    EXPECT_EQ(optionalResponse.value(), response);
}

TEST_F(ETLExtractorTest, CallsPipeFinishWithInitialSequenceAtExit)
{
    EXPECT_CALL(dataPipe_, finish(123)).Times(1);
    state_.isStopping = true;

    extractor_ = std::make_unique<ExtractorType>(dataPipe_, networkValidatedLedgers_, ledgerFetcher_, 123, 234, state_);
}
