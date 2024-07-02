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

    MOCK_METHOD((std::map<std::string, data::Amendment> const&), getSupported, (), (const, override));

    MOCK_METHOD(std::vector<data::Amendment> const&, getAll, (), (const, override));

    MOCK_METHOD(bool, isEnabled, (data::AmendmentKey const&, uint32_t), (const, override));

    MOCK_METHOD(bool, isEnabled, (boost::asio::yield_context, data::AmendmentKey const&, uint32_t), (const, override));

    MOCK_METHOD(data::Amendment const&, getAmendment, (data::AmendmentKey const&), (const, override));

    MOCK_METHOD(data::Amendment const&, IndexOperator, (data::AmendmentKey const&), (const));

    data::Amendment const&
    operator[](data::AmendmentKey const& key) const override
    {
        return IndexOperator(key);
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
