#pragma once

#include "etl/LedgerFetcherInterface.hpp"
#include "util/FakeFetchResponse.hpp"

#include <gmock/gmock.h>

#include <cstdint>
#include <optional>

struct MockLedgerFetcher : etl::LedgerFetcherInterface {
    MOCK_METHOD(OptionalGetLedgerResponseType, fetchData, (uint32_t), (override));
    MOCK_METHOD(OptionalGetLedgerResponseType, fetchDataAndDiff, (uint32_t), (override));
};
