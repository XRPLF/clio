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
    MOCK_METHOD(bool, isFallbackRecovery, (), (const, override));
    MOCK_METHOD(void, setFallbackRecovery, (bool), (override));
    MOCK_METHOD(bool, isEtlStarted, (), (const, override));
    MOCK_METHOD(bool, isCacheFull, (), (const, override));
    MOCK_METHOD(std::unique_ptr<etl::WriterStateInterface>, clone, (), (const, override));
};

using MockWriterState = testing::StrictMock<MockWriterStateBase>;
using NiceMockWriterState = testing::NiceMock<MockWriterStateBase>;
