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

#pragma once

#include "util/Fixtures.h"
#include "util/MockWsBase.h"
#include "util/Taggable.h"
#include "util/config/Config.h"
#include "web/interface/ConnectionBase.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

// Base class for feed tests, providing easy way to access the received feed
template <typename TestedFeed>
class FeedBaseTest : public SyncAsioContextTest, public MockBackendTest {
protected:
    util::TagDecoratorFactory tagDecoratorFactory{util::Config{}};
    std::shared_ptr<web::ConnectionBase> sessionPtr;
    std::shared_ptr<TestedFeed> testFeedPtr;

    void
    SetUp() override
    {
        SyncAsioContextTest::SetUp();
        MockBackendTest::SetUp();
        testFeedPtr = std::make_shared<TestedFeed>(ctx);
        sessionPtr = std::make_shared<MockSession>(tagDecoratorFactory);
    }

    void
    TearDown() override
    {
        sessionPtr.reset();
        testFeedPtr.reset();
        MockBackendTest::TearDown();
        SyncAsioContextTest::TearDown();
    }

    std::string const&
    receivedFeedMessage() const
    {
        auto const mockSession = dynamic_cast<MockSession*>(sessionPtr.get());
        [&] { ASSERT_NE(mockSession, nullptr); }();
        return mockSession->message;
    }

    void
    cleanReceivedFeed()
    {
        auto mockSession = dynamic_cast<MockSession*>(sessionPtr.get());
        [&] { ASSERT_NE(mockSession, nullptr); }();
        mockSession->message.clear();
    }
};
