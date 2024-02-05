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

#pragma once

#include "etl/ETLState.hpp"

#include <boost/json.hpp>
#include <boost/json/object.hpp>
#include <gmock/gmock.h>

#include <chrono>
#include <cstdint>
#include <optional>

struct MockETLService {
    MOCK_METHOD(boost::json::object, getInfo, (), (const));
    MOCK_METHOD(std::chrono::time_point<std::chrono::system_clock>, getLastPublish, (), (const));
    MOCK_METHOD(std::uint32_t, lastPublishAgeSeconds, (), (const));
    MOCK_METHOD(std::uint32_t, lastCloseAgeSeconds, (), (const));
    MOCK_METHOD(bool, isAmendmentBlocked, (), (const));
    MOCK_METHOD(std::optional<etl::ETLState>, getETLState, (), (const));
};
