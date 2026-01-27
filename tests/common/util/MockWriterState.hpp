//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#include "etl/WriterState.hpp"

#include <gmock/gmock.h>

#include <memory>

struct MockWriterStateBase : public etl::WriterStateInterface {
    MOCK_METHOD(bool, isReadOnly, (), (const, override));
    MOCK_METHOD(bool, isWriting, (), (const, override));
    MOCK_METHOD(void, startWriting, (), (override));
    MOCK_METHOD(void, giveUpWriting, (), (override));
    MOCK_METHOD(void, setWriterDecidingFallback, (), (override));
    MOCK_METHOD(bool, isFallback, (), (const, override));
    MOCK_METHOD(bool, isLoadingCache, (), (const, override));
    MOCK_METHOD(std::unique_ptr<etl::WriterStateInterface>, clone, (), (const, override));
};

using MockWriterState = testing::StrictMock<MockWriterStateBase>;
using NiceMockWriterState = testing::NiceMock<MockWriterStateBase>;
