#pragma once

#include "util/FakeFetchResponse.hpp"

#include <gmock/gmock.h>

#include <cstdint>
#include <optional>

struct MockExtractionDataPipe {
    MOCK_METHOD(void, push, (uint32_t, std::optional<FakeFetchResponse>&&), ());
    MOCK_METHOD(std::optional<FakeFetchResponse>, popNext, (uint32_t), ());
    MOCK_METHOD(uint32_t, getStride, (), (const));
    MOCK_METHOD(void, finish, (uint32_t), ());
    MOCK_METHOD(void, cleanup, (), ());
};
