#pragma once

#include "etl/AmendmentBlockHandlerInterface.hpp"

#include <gmock/gmock.h>

struct MockAmendmentBlockHandler : etl::AmendmentBlockHandlerInterface {
    MOCK_METHOD(void, notifyAmendmentBlocked, (), (override));
    MOCK_METHOD(void, stop, (), (override));
};
