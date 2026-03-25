#include "util/LoggerFixtures.hpp"
#include "util/TerminationHandler.hpp"

#include <gtest/gtest.h>

int
main(int argc, char* argv[])
{
    util::setTerminationHandler();
    testing::InitGoogleTest(&argc, argv);

    LoggerFixture::init();

    return RUN_ALL_TESTS();
}
