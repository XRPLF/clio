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

#include "data/LedgerCacheInterface.hpp"
#include "data/Types.hpp"
#include "etl/Models.hpp"

#include <gmock/gmock.h>
#include <xrpl/basics/base_uint.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct MockLedgerCache : data::LedgerCacheInterface {
    MOCK_METHOD(
        void,
        updateImpl,
        (std::vector<data::LedgerObject> const& a, uint32_t b, bool c),
        ()
    );

    void
    update(std::vector<data::LedgerObject> const& a, uint32_t b, bool c = false) override
    {
        updateImpl(a, b, c);
    }

    MOCK_METHOD(
        std::optional<data::Blob>,
        get,
        (ripple::uint256 const& a, uint32_t b),
        (const, override)
    );

    MOCK_METHOD(void, update, (std::vector<etl::model::Object> const&, uint32_t), (override));

    MOCK_METHOD(
        std::optional<data::Blob>,
        getDeleted,
        (ripple::uint256 const&, uint32_t),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<data::LedgerObject>,
        getSuccessor,
        (ripple::uint256 const& a, uint32_t b),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<data::LedgerObject>,
        getPredecessor,
        (ripple::uint256 const& a, uint32_t b),
        (const, override)
    );

    MOCK_METHOD(void, setDisabled, (), (override));

    MOCK_METHOD(bool, isDisabled, (), (const, override));

    MOCK_METHOD(void, setFull, (), (override));

    MOCK_METHOD(bool, isFull, (), (const, override));

    MOCK_METHOD(uint32_t, latestLedgerSequence, (), (const, override));

    MOCK_METHOD(size_t, size, (), (const, override));

    MOCK_METHOD(float, getObjectHitRate, (), (const, override));

    MOCK_METHOD(float, getSuccessorHitRate, (), (const, override));

    MOCK_METHOD(void, waitUntilCacheContainsSeq, (uint32_t), (override));

    using SaveToFileReturnType = std::expected<void, std::string>;
    MOCK_METHOD(SaveToFileReturnType, saveToFile, (std::string const& path), (const, override));

    using LoadFromFileReturnType = std::expected<void, std::string>;
    MOCK_METHOD(
        LoadFromFileReturnType,
        loadFromFile,
        (std::string const& path, uint32_t minLatestSequence),
        (override)
    );
};
