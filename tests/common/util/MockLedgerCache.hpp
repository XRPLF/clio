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
        (xrpl::uint256 const& a, uint32_t b),
        (const, override)
    );

    MOCK_METHOD(void, update, (std::vector<etl::model::Object> const&, uint32_t), (override));

    MOCK_METHOD(
        std::optional<data::Blob>,
        getDeleted,
        (xrpl::uint256 const&, uint32_t),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<data::LedgerObject>,
        getSuccessor,
        (xrpl::uint256 const& a, uint32_t b),
        (const, override)
    );

    MOCK_METHOD(
        std::optional<data::LedgerObject>,
        getPredecessor,
        (xrpl::uint256 const& a, uint32_t b),
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

    MOCK_METHOD(void, startLoading, (), (override));

    MOCK_METHOD(bool, isCurrentlyLoading, (), (const, override));
};
