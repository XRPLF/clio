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
#include "etl/impl/Transformer.hpp"
#include "util/FakeFetchResponse.hpp"
#include "util/Fixtures.hpp"
#include "util/MockAmendmentBlockHandler.hpp"
#include "util/MockExtractionDataPipe.hpp"
#include "util/MockLedgerLoader.hpp"
#include "util/MockLedgerPublisher.hpp"
#include "util/StringUtils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <thread>

using namespace testing;
using namespace etl;

// taken from BackendTests
constexpr static auto RAW_HEADER =
    "03C3141A01633CD656F91B4EBB5EB89B791BD34DBC8A04BB6F407C5335BC54351E"
    "DD733898497E809E04074D14D271E4832D7888754F9230800761563A292FA2315A"
    "6DB6FE30CC5909B285080FCD6773CC883F9FE0EE4D439340AC592AADB973ED3CF5"
    "3E2232B33EF57CECAC2816E3122816E31A0A00F8377CD95DFA484CFAE282656A58"
    "CE5AA29652EFFD80AC59CD91416E4E13DBBE";

class ETLTransformerTest : public MockBackendTest {
protected:
    using DataType = FakeFetchResponse;
    using ExtractionDataPipeType = MockExtractionDataPipe;
    using LedgerLoaderType = MockLedgerLoader;
    using LedgerPublisherType = MockLedgerPublisher;
    using AmendmentBlockHandlerType = MockAmendmentBlockHandler;
    using TransformerType = etl::impl::
        Transformer<ExtractionDataPipeType, LedgerLoaderType, LedgerPublisherType, AmendmentBlockHandlerType>;

    ExtractionDataPipeType dataPipe_;
    LedgerLoaderType ledgerLoader_;
    LedgerPublisherType ledgerPublisher_;
    AmendmentBlockHandlerType amendmentBlockHandler_;
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

TEST_F(ETLTransformerTest, StopsOnWriteConflict)
{
    state_.writeConflict = true;

    EXPECT_CALL(dataPipe_, popNext).Times(0);
    EXPECT_CALL(ledgerPublisher_, publish(_)).Times(0);

    transformer_ = std::make_unique<TransformerType>(
        dataPipe_, backend, ledgerLoader_, ledgerPublisher_, amendmentBlockHandler_, 0, state_
    );

    transformer_->waitTillFinished();  // explicitly joins the thread
}

TEST_F(ETLTransformerTest, StopsOnEmptyFetchResponse)
{
    backend->cache().setFull();  // to avoid throwing exception in updateCache

    auto const blob = hexStringToBinaryString(RAW_HEADER);
    auto const response = std::make_optional<FakeFetchResponse>(blob);

    ON_CALL(dataPipe_, popNext).WillByDefault([this, &response](auto) -> std::optional<FakeFetchResponse> {
        if (state_.isStopping)
            return std::nullopt;
        return response;  // NOLINT (performance-no-automatic-move)
    });
    ON_CALL(*backend, doFinishWrites).WillByDefault(Return(true));

    // TODO: most of this should be hidden in a smaller entity that is injected into the transformer thread
    EXPECT_CALL(dataPipe_, popNext).Times(AtLeast(1));
    EXPECT_CALL(*backend, startWrites).Times(AtLeast(1));
    EXPECT_CALL(*backend, writeLedger(_, _)).Times(AtLeast(1));
    EXPECT_CALL(ledgerLoader_, insertTransactions).Times(AtLeast(1));
    EXPECT_CALL(*backend, writeAccountTransactions).Times(AtLeast(1));
    EXPECT_CALL(*backend, writeNFTs).Times(AtLeast(1));
    EXPECT_CALL(*backend, writeNFTTransactions).Times(AtLeast(1));
    EXPECT_CALL(*backend, doFinishWrites).Times(AtLeast(1));
    EXPECT_CALL(ledgerPublisher_, publish(_)).Times(AtLeast(1));

    transformer_ = std::make_unique<TransformerType>(
        dataPipe_, backend, ledgerLoader_, ledgerPublisher_, amendmentBlockHandler_, 0, state_
    );

    // after 10ms we start spitting out empty responses which means the extractor is finishing up
    // this is normally combined with stopping the entire thing by setting the isStopping flag.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    state_.isStopping = true;
}

TEST_F(ETLTransformerTest, DoesNotPublishIfCanNotBuildNextLedger)
{
    backend->cache().setFull();  // to avoid throwing exception in updateCache

    auto const blob = hexStringToBinaryString(RAW_HEADER);
    auto const response = std::make_optional<FakeFetchResponse>(blob);

    ON_CALL(dataPipe_, popNext).WillByDefault(Return(response));
    ON_CALL(*backend, doFinishWrites).WillByDefault(Return(false));  // emulate write failure

    // TODO: most of this should be hidden in a smaller entity that is injected into the transformer thread
    EXPECT_CALL(dataPipe_, popNext).Times(AtLeast(1));
    EXPECT_CALL(*backend, startWrites).Times(AtLeast(1));
    EXPECT_CALL(*backend, writeLedger(_, _)).Times(AtLeast(1));
    EXPECT_CALL(ledgerLoader_, insertTransactions).Times(AtLeast(1));
    EXPECT_CALL(*backend, writeAccountTransactions).Times(AtLeast(1));
    EXPECT_CALL(*backend, writeNFTs).Times(AtLeast(1));
    EXPECT_CALL(*backend, writeNFTTransactions).Times(AtLeast(1));
    EXPECT_CALL(*backend, doFinishWrites).Times(AtLeast(1));

    // should not call publish
    EXPECT_CALL(ledgerPublisher_, publish(_)).Times(0);

    transformer_ = std::make_unique<TransformerType>(
        dataPipe_, backend, ledgerLoader_, ledgerPublisher_, amendmentBlockHandler_, 0, state_
    );
}

// TODO: implement tests for amendment block. requires more refactoring
