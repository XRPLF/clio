#pragma once

#include "util/AsioContextTestFixture.hpp"
#include "util/MockAmendmentCenter.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockWsBase.hpp"
#include "util/SyncExecutionCtxFixture.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

// Base class for feed tests, providing easy way to access the received feed
// The interface for matchers is from gtest so we don't want to change the casing
// NOLINTBEGIN(readability-identifier-naming)

template <typename TestedFeed>
struct FeedBaseTest : util::prometheus::WithPrometheus, MockBackendTest, SyncExecutionCtxFixture {
protected:
    web::SubscriptionContextPtr sessionPtr = std::make_shared<MockSession>();
    std::shared_ptr<TestedFeed> testFeedPtr = std::make_shared<TestedFeed>(ctx_);
    MockSession* mockSessionPtr = dynamic_cast<MockSession*>(sessionPtr.get());
    StrictMockAmendmentCenterSharedPtr mockAmendmentCenterPtr_;
    uint32_t const networkID{123};
};

namespace feed::impl {
class SharedStringJsonEqMatcher {
    std::string expected_;

public:
    using is_gtest_matcher = void;

    explicit SharedStringJsonEqMatcher(std::string expected) : expected_(std::move(expected))
    {
    }

    bool
    MatchAndExplain(std::shared_ptr<std::string> const& arg, std::ostream* /* listener */) const
    {
        return boost::json::parse(*arg) == boost::json::parse(expected_);
    }

    void
    DescribeTo(std::ostream* os) const
    {
        *os << "Contains json " << expected_;
    }

    void
    DescribeNegationTo(std::ostream* os) const
    {
        *os << "Expecting json " << expected_;
    }
};
}  // namespace feed::impl

// NOLINTEND(readability-identifier-naming)

inline ::testing::Matcher<std::shared_ptr<std::string>>
sharedStringJsonEq(std::string const& expected)
{
    return feed::impl::SharedStringJsonEqMatcher(expected);
}
