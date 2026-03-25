#pragma once

#include "data/AmendmentCenterInterface.hpp"
#include "data/Types.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/object.hpp>
#include <gmock/gmock.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct MockAmendmentCenter : public data::AmendmentCenterInterface {
    MOCK_METHOD(bool, isSupported, (data::AmendmentKey const&), (const, override));

    MOCK_METHOD(
        (std::map<std::string, data::Amendment> const&),
        getSupported,
        (),
        (const, override)
    );

    MOCK_METHOD(std::vector<data::Amendment> const&, getAll, (), (const, override));

    MOCK_METHOD(bool, isEnabled, (data::AmendmentKey const&, uint32_t), (const, override));

    MOCK_METHOD(
        bool,
        isEnabled,
        (boost::asio::yield_context, data::AmendmentKey const&, uint32_t),
        (const, override)
    );

    MOCK_METHOD(
        std::vector<bool>,
        isEnabled,
        (boost::asio::yield_context, std::vector<data::AmendmentKey> const&, uint32_t),
        (const, override)
    );

    MOCK_METHOD(
        data::Amendment const&,
        getAmendment,
        (data::AmendmentKey const&),
        (const, override)
    );

    MOCK_METHOD(data::Amendment const&, indexOperator, (data::AmendmentKey const&), (const));

    data::Amendment const&
    operator[](data::AmendmentKey const& key) const override
    {
        return indexOperator(key);
    }
};

template <template <typename> typename MockType = ::testing::NiceMock>
struct MockAmendmentCenterSharedPtrImpl {
    std::shared_ptr<MockType<MockAmendmentCenter>> amendmentCenterMock =
        std::make_shared<MockType<MockAmendmentCenter>>();

    operator std::shared_ptr<data::AmendmentCenterInterface>()
    {
        return amendmentCenterMock;
    }

    operator std::shared_ptr<data::AmendmentCenterInterface const>()
    {
        return amendmentCenterMock;
    }

    MockType<MockAmendmentCenter>&
    operator*()
    {
        return *amendmentCenterMock;
    }
};

using MockAmendmentCenterSharedPtr = MockAmendmentCenterSharedPtrImpl<>;
using StrictMockAmendmentCenterSharedPtr = MockAmendmentCenterSharedPtrImpl<testing::StrictMock>;
