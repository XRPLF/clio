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

#include <etl/impl/Transformer.h>
#include <util/FakeFetchResponse.h>
#include <util/Fixtures.h>
#include <util/MockExtractionDataPipe.h>
#include <util/MockLedgerLoader.h>
#include <util/MockLedgerPublisher.h>

#include <gtest/gtest.h>

#include <memory>

using namespace testing;

class ETLTransformerTest : public MockBackendTest
{
protected:
    using DataType = FakeFetchResponse;
    using ExtractionDataPipeType = MockExtractionDataPipe<DataType>;
    using LedgerLoaderType = MockLedgerLoader<DataType>;
    using LedgerPublisherType = MockLedgerPublisher;
    using TransformerType = clio::detail::Transformer<ExtractionDataPipeType, LedgerLoaderType, LedgerPublisherType>;

    ExtractionDataPipeType dataPipe_;
    LedgerLoaderType ledgerLoader_;
    LedgerPublisherType ledgerPublisher_;
    SystemState state_;

    std::unique_ptr<TransformerType> transformer_;

public:
    void
    SetUp() override
    {
        MockBackendTest::SetUp();
        state_.isStopping = false;
        state_.writeConflict = false;
        state_.isReadOnly = false;
        state_.isWriting = false;
    }

    void
    TearDown() override
    {
        transformer_.reset();
        MockBackendTest::TearDown();
    }
};

TEST_F(ETLTransformerTest, Tmp)
{
    // ON_CALL(dataPipe_, getStride).WillByDefault(Return(4));
    // EXPECT_CALL(dataPipe_, getStride).Times(3);

    // auto response = FakeFetchResponse{};
    // ON_CALL(ledgerFetcher_, fetchDataAndDiff(_)).WillByDefault(Return(response));
    // EXPECT_CALL(ledgerFetcher_, fetchDataAndDiff).Times(3);
    // EXPECT_CALL(dataPipe_, push).Times(3);
    // EXPECT_CALL(dataPipe_, finish(0)).Times(1);

    transformer_ =
        std::make_unique<TransformerType>(dataPipe_, mockBackendPtr, ledgerLoader_, ledgerPublisher_, 0, state_);
}
