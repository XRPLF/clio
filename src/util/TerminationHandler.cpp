#include "util/TerminationHandler.hpp"

#include "util/log/Logger.hpp"

#ifndef CLIO_WITHOUT_STACKTRACE
#include <boost/stacktrace/stacktrace.hpp>
#endif  // CLIO_WITHOUT_STACKTRACE

#include <cstdlib>
#include <exception>

namespace util {

namespace {

void
terminationHandler()
{
#ifndef CLIO_WITHOUT_STACKTRACE
    try {
        LOG(LogService::fatal()) << "Exit on terminate. Backtrace:\n"
                                 << boost::stacktrace::stacktrace();
    } catch (...) {
        LOG(LogService::fatal()) << "Exit on terminate. Can't get backtrace.";
    }
#else
    LOG(LogService::fatal()) << "Exit on terminate. Stacktrace disabled.";
#endif  // CLIO_WITHOUT_STACKTRACE

    // We're calling std::abort later, so spdlog won't be shutdown automatically
    util::LogService::shutdown();
    std::abort();
}

}  // namespace

void
setTerminationHandler()
{
    std::set_terminate(terminationHandler);
}

}  // namespace util
