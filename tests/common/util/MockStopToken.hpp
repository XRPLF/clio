#pragma once

#include <gmock/gmock.h>

struct MockStopSource {
    MOCK_METHOD(void, requestStop, (), ());
};

struct MockStopToken {
    MOCK_METHOD(bool, isStopRequested, (), (const));
};
