#pragma once

#include "etl/ETLState.hpp"
#include "etl/InitialLoadObserverInterface.hpp"
#include "etl/LoadBalancerInterface.hpp"
#include "rpc/Errors.hpp"
#include "util/FakeFetchResponse.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <gmock/gmock.h>

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

struct MockLoadBalancer : etl::LoadBalancerInterface {
    using RawLedgerObjectType = FakeLedgerObject;

    MOCK_METHOD(
        etl::InitialLedgerLoadResult,
        loadInitialLedger,
        (uint32_t, etl::InitialLoadObserverInterface&, std::chrono::steady_clock::duration),
        (override)
    );
    MOCK_METHOD(
        OptionalGetLedgerResponseType,
        fetchLedger,
        (uint32_t, bool, bool, std::chrono::steady_clock::duration),
        (override)
    );
    MOCK_METHOD(boost::json::value, toJson, (), (const, override));
    MOCK_METHOD(std::optional<etl::ETLState>, getETLState, (), (noexcept, override));

    using ForwardToRippledReturnType = std::expected<boost::json::object, rpc::CombinedError>;
    MOCK_METHOD(
        ForwardToRippledReturnType,
        forwardToRippled,
        (boost::json::object const&,
         std::optional<std::string> const&,
         bool,
         boost::asio::yield_context),
        (override)
    );
    MOCK_METHOD(void, stop, (boost::asio::yield_context), ());
};
