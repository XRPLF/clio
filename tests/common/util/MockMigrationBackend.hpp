#pragma once

#include "util/MockBackend.hpp"

struct MockMigrationBackend : public MockBackend {
    using MockBackend::MockBackend;
};
