#pragma once

#include <gmock/gmock.h>

template <typename ValueType>
struct MockOperation {
    MOCK_METHOD(void, wait, (), (const));
    MOCK_METHOD(ValueType, get, (), (const));
};

template <typename ValueType>
struct MockStoppableOperation {
    MOCK_METHOD(void, requestStop, (), (const));
    MOCK_METHOD(void, wait, (), (const));
    MOCK_METHOD(ValueType, get, (), (const));
};

template <typename ValueType>
struct MockScheduledOperation {
    MOCK_METHOD(void, cancel, (), (const));
    MOCK_METHOD(void, requestStop, (), (const));
    MOCK_METHOD(void, wait, (), (const));
    MOCK_METHOD(ValueType, get, (), (const));
    MOCK_METHOD(void, getToken, (), (const));
};

template <typename ValueType>
struct MockRepeatingOperation {
    MOCK_METHOD(void, requestStop, (), (const));
    MOCK_METHOD(void, wait, (), (const));
    MOCK_METHOD(ValueType, get, (), (const));
    MOCK_METHOD(void, invoke, (), (const));
};
