#pragma once

#include "util/Taggable.hpp"
#include "util/config/ConfigDefinition.hpp"
#include "util/config/ConfigValue.hpp"
#include "util/config/Types.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/beast/http/status.hpp>
#include <gmock/gmock.h>

#include <cstdint>
#include <memory>
#include <string>

struct MockSession : public web::SubscriptionContextInterface {
    MOCK_METHOD(void, send, (std::shared_ptr<std::string>), (override));
    MOCK_METHOD(void, onDisconnect, (OnDisconnectSlot const&), (override));
    MOCK_METHOD(void, setApiSubversion, (uint32_t), (override));
    MOCK_METHOD(uint32_t, apiSubversion, (), (const, override));

    util::TagDecoratorFactory tagDecoratorFactory{util::config::ClioConfigDefinition{
        {"log.tag_style",
         util::config::ConfigValue{util::config::ConfigType::String}.defaultValue("none")}
    }};

    MockSession() : web::SubscriptionContextInterface(tagDecoratorFactory)
    {
    }
};

struct MockDeadSession : public web::ConnectionBase {
    void
    send(std::shared_ptr<std::string>) override
    {
        // err happen, the session should remove from subscribers
        ec_.assign(2, boost::system::system_category());
    }

    void
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    send(std::string&&, boost::beast::http::status = boost::beast::http::status::ok) override
    {
    }

    MockDeadSession(util::TagDecoratorFactory const& factory) : web::ConnectionBase(factory, "")
    {
    }
};
