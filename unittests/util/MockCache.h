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

#include <backend/Types.h>

#include <gmock/gmock.h>

struct MockCache
{
    MOCK_METHOD(void, update, (std::vector<Backend::LedgerObject> const& a, uint32_t b, bool c), ());

    MOCK_METHOD(std::optional<Backend::Blob>, get, (ripple::uint256 const& a, uint32_t b), (const));

    MOCK_METHOD(std::optional<Backend::LedgerObject>, getSuccessor, (ripple::uint256 const& a, uint32_t b), (const));

    MOCK_METHOD(std::optional<Backend::LedgerObject>, getPredecessor, (ripple::uint256 const& a, uint32_t b), (const));

    MOCK_METHOD(void, setDisabled, (), ());

    MOCK_METHOD(void, setFull, (), ());

    MOCK_METHOD(bool, isFull, (), (const));

    MOCK_METHOD(uint32_t, latestLedgerSequence, (), (const));

    MOCK_METHOD(size_t, size, (), (const));

    MOCK_METHOD(float, getObjectHitRate, (), (const));

    MOCK_METHOD(float, getSuccessorHitRate, (), (const));
};
