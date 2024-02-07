//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "util/TerminationHandler.hpp"

#include "util/log/Logger.hpp"

#include <boost/stacktrace/stacktrace.hpp>

#include <cstdlib>
#include <exception>

namespace util {

namespace {

void
terminationHandler()
{
    try {
        LOG(LogService::fatal()) << "Exit on terminate. Backtrace:\n" << boost::stacktrace::stacktrace();
    } catch (...) {
        LOG(LogService::fatal()) << "Exit on terminate. Can't get backtrace.";
    }
    std::abort();
}

}  // namespace

void
setTerminationHandler()
{
    std::set_terminate(terminationHandler);
}

}  // namespace util
