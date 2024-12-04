//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "rpc/common/Types.hpp"
#include "rpc/handlers/AMMInfo.hpp"
#include "rpc/handlers/AccountChannels.hpp"
#include "rpc/handlers/AccountCurrencies.hpp"
#include "rpc/handlers/AccountInfo.hpp"
#include "rpc/handlers/AccountLines.hpp"
#include "rpc/handlers/AccountNFTs.hpp"
#include "rpc/handlers/AccountObjects.hpp"
#include "rpc/handlers/AccountOffers.hpp"
#include "rpc/handlers/AccountTx.hpp"
#include "rpc/handlers/BookChanges.hpp"
#include "rpc/handlers/BookOffers.hpp"
#include "rpc/handlers/DepositAuthorized.hpp"
#include "rpc/handlers/Feature.hpp"
#include "rpc/handlers/GatewayBalances.hpp"
#include "rpc/handlers/GetAggregatePrice.hpp"
#include "rpc/handlers/Ledger.hpp"
#include "rpc/handlers/LedgerData.hpp"
#include "rpc/handlers/LedgerEntry.hpp"
#include "rpc/handlers/LedgerIndex.hpp"
#include "rpc/handlers/MPTHolders.hpp"
#include "rpc/handlers/NFTBuyOffers.hpp"
#include "rpc/handlers/NFTHistory.hpp"
#include "rpc/handlers/NFTInfo.hpp"
#include "rpc/handlers/NFTSellOffers.hpp"
#include "rpc/handlers/NFTsByIssuer.hpp"
#include "rpc/handlers/NoRippleCheck.hpp"
#include "rpc/handlers/ServerInfo.hpp"
#include "rpc/handlers/Subscribe.hpp"
#include "rpc/handlers/TransactionEntry.hpp"
#include "util/Assert.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/MockAmendmentCenter.hpp"
#include "util/MockCounters.hpp"
#include "util/MockCountersFixture.hpp"
#include "util/MockETLService.hpp"
#include "util/MockETLServiceTestFixture.hpp"
#include "util/MockLoadBalancer.hpp"
#include "util/MockSubscriptionManager.hpp"
#include "util/MockWsBase.hpp"
#include "util/TestObject.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/asio/spawn.hpp>
#include <fmt/core.h>
#include <gtest/gtest.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Book.h>
#include <xrpl/protocol/UintTypes.h>

#include <memory>
#include <string>
#include <vector>

using ::testing::Types;
using namespace rpc;
using TestServerInfoHandler = BaseServerInfoHandler<MockLoadBalancer, MockETLService, MockCounters>;

constexpr static auto Index1 = "05FB0EB4B899F056FA095537C5817163801F544BAFCEA39C995D76DB4D16F9DD";
constexpr static auto AmmAccount = "rLcS7XL6nxRAi7JcbJcn1Na179oF3vdfbh";
constexpr static auto Account = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto NftID = "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004";
constexpr static auto Currency = "0158415500000000C1F76FF6ECB0BAC600000000";

using AnyHandlerType = Types<
    AccountChannelsHandler,
    AccountCurrenciesHandler,
    AccountInfoHandler,
    AccountLinesHandler,
    AccountNFTsHandler,
    AccountObjectsHandler,
    AccountOffersHandler,
    AccountTxHandler,
    AMMInfoHandler,
    BookChangesHandler,
    BookOffersHandler,
    DepositAuthorizedHandler,
    FeatureHandler,
    GatewayBalancesHandler,
    GetAggregatePriceHandler,
    LedgerHandler,
    LedgerDataHandler,
    LedgerEntryHandler,
    LedgerIndexHandler,
    MPTHoldersHandler,
    NFTsByIssuerHandler,
    NFTHistoryHandler,
    NFTBuyOffersHandler,
    NFTInfoHandler,
    NFTSellOffersHandler,
    NoRippleCheckHandler,
    TestServerInfoHandler,
    SubscribeHandler,
    TransactionEntryHandler>;

template <typename HandlerType>
struct AllHandlersDeathTest : HandlerBaseTest,
                              MockLoadBalancerTest,
                              MockCountersTest,
                              testing::WithParamInterface<std::string> {
    AllHandlersDeathTest() : handler_{initHandler()}
    {
        ASSERT(mockAmendmentCenterPtr.amendmentCenterMock != nullptr, "mockAmendmentCenterPtr is not initialized.");
        ASSERT(mockSubscriptionManagerPtr.subscriptionManagerMock != nullptr, "mockSubscriptionPtr is not initialized");
    }

    web::SubscriptionContextPtr session_ = std::make_shared<MockSession>();
    MockSession* mockSession_ = dynamic_cast<MockSession*>(session_.get());
    StrictMockSubscriptionManagerSharedPtr mockSubscriptionManagerPtr;
    StrictMockAmendmentCenterSharedPtr mockAmendmentCenterPtr;
    HandlerType handler_;

private:
    HandlerType
    initHandler()
    {
        if constexpr (std::is_same_v<HandlerType, AccountInfoHandler> || std::is_same_v<HandlerType, FeatureHandler>) {
            return HandlerType{this->backend, this->mockAmendmentCenterPtr};
        } else if constexpr (std::is_same_v<HandlerType, SubscribeHandler>) {
            return HandlerType{this->backend, this->mockSubscriptionManagerPtr};
        } else if constexpr (std::is_same_v<HandlerType, TestServerInfoHandler>) {
            return HandlerType{
                this->backend,
                this->mockSubscriptionManagerPtr,
                mockLoadBalancerPtr,
                mockETLServicePtr,
                *mockCountersPtr
            };
        } else {
            return HandlerType{this->backend};
        }
    }
};

template <typename Handler>
Handler::Input
createInput()
{
    return typename Handler::Input{};
}

// need to set specific values for input for some handler's to pass checks in .process() function
template <>
AccountInfoHandler::Input
createInput<AccountInfoHandler>()
{
    AccountInfoHandler::Input input{};
    input.account = Account;
    input.ident = "asdf";
    return input;
}

template <>
AMMInfoHandler::Input
createInput<AMMInfoHandler>()
{
    AMMInfoHandler::Input input{};
    input.ammAccount = GetAccountIDWithString(AmmAccount);
    return input;
}

template <>
BookOffersHandler::Input
createInput<BookOffersHandler>()
{
    BookOffersHandler::Input input{};
    input.paysCurrency = ripple::xrpCurrency();
    input.getsCurrency = ripple::Currency(Currency);
    input.paysID = ripple::xrpAccount();
    input.getsID = GetAccountIDWithString(Account);

    return input;
}

template <>
LedgerEntryHandler::Input
createInput<LedgerEntryHandler>()
{
    LedgerEntryHandler::Input input{};
    input.index = Index1;
    return input;
}

template <>
NFTBuyOffersHandler::Input
createInput<NFTBuyOffersHandler>()
{
    NFTBuyOffersHandler::Input input{};
    input.nftID = NftID;
    return input;
}

template <>
NFTInfoHandler::Input
createInput<NFTInfoHandler>()
{
    NFTInfoHandler::Input input{};
    input.nftID = NftID;
    return input;
}

template <>
NFTSellOffersHandler::Input
createInput<NFTSellOffersHandler>()
{
    NFTSellOffersHandler::Input input{};
    input.nftID = NftID;
    return input;
}

template <>
SubscribeHandler::Input
createInput<SubscribeHandler>()
{
    SubscribeHandler::Input input{};

    input.books =
        std::vector<SubscribeHandler::OrderBook>{SubscribeHandler::OrderBook{ripple::Book{}, Account, true, true}};
    return input;
}

TYPED_TEST_CASE(AllHandlersDeathTest, AnyHandlerType);

TYPED_TEST(AllHandlersDeathTest, NoRangeAvailable)
{
    // doesn't work without 'this'
    this->runSpawn(
        [&](boost::asio::yield_context yield) {
            TypeParam const handler = this->handler_;

            auto const input = createInput<TypeParam>();
            auto const context = Context{yield, this->session_};

            EXPECT_DEATH(
                { [[maybe_unused]] auto _unused = handler.process(input, context); }, "Assertion .* failed at .*"
            );
        },
        true
    );
}
