#include <util/Fixtures.h>
using namespace clio;

// Used as a fixture for tests with enabled logging
class LoggerTest : public LoggerFixture
{
};

// Used as a fixture for tests with disabled logging
class NoLoggerTest : public NoLoggerFixture
{
};

TEST_F(LoggerTest, Basic)
{
    Logger log{"General"};
    log.info() << "Info line logged";
    checkEqual("General:NFO Info line logged");

    LogService::debug() << "Debug line with numbers " << 12345;
    checkEqual("General:DBG Debug line with numbers 12345");

    LogService::warn() << "Warning is logged";
    checkEqual("General:WRN Warning is logged");
}

TEST_F(LoggerTest, Filtering)
{
    Logger log{"General"};
    log.trace() << "Should not be logged";
    checkEmpty();

    log.warn() << "Warning is logged";
    checkEqual("General:WRN Warning is logged");

    Logger tlog{"Trace"};
    tlog.trace() << "Trace line logged for 'Trace' component";
    checkEqual("Trace:TRC Trace line logged for 'Trace' component");
}

TEST_F(NoLoggerTest, Basic)
{
    Logger log{"Trace"};
    log.trace() << "Nothing";
    checkEmpty();

    LogService::fatal() << "Still nothing";
    checkEmpty();
}
