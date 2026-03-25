#pragma once

#include "util/Taggable.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/beast/http/status.hpp>
#include <gmock/gmock.h>

#include <memory>
#include <string>

struct ConnectionBaseMock : web::ConnectionBase {
    using ConnectionBase::ConnectionBase;

    MOCK_METHOD(void, send, (std::string&&, boost::beast::http::status), (override));
    MOCK_METHOD(void, send, (std::shared_ptr<std::string>), (override));
    MOCK_METHOD(
        web::SubscriptionContextPtr,
        makeSubscriptionContext,
        (util::TagDecoratorFactory const& factory),
        (override)
    );
    MOCK_METHOD(void, sendSlowDown, (std::string const&), (override));
};

using ConnectionBaseStrictMockPtr = std::shared_ptr<testing::StrictMock<ConnectionBaseMock>>;
