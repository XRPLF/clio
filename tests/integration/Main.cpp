#include "util/LoggerFixtures.hpp"
#include "util/TerminationHandler.hpp"

#include <TestGlobals.hpp>
#include <gtest/gtest.h>

/*
 * Supported custom command line options for clio_tests:
 *   --backend_host=<host>         - sets the cassandra/scylladb host for backend tests
 *   --backend_keyspace=<keyspace> - sets the cassandra/scylladb keyspace for backend tests
 *   --clean-gcda                  - delete all gcda files before running tests
 */
int
main(int argc, char* argv[])
{
    util::setTerminationHandler();
    testing::InitGoogleTest(&argc, argv);
    LoggerFixture::init();

    TestGlobals::instance().parse(argc, argv);

    return RUN_ALL_TESTS();
}
