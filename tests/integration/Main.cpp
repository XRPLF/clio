//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

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

#include "TestGlobals.hpp"
#include "util/TerminationHandler.hpp"

#include <gtest/gtest.h>

/*
 * Supported custom command line options for clio_tests:
 *   --backend_host=<host>         - sets the cassandra/scylladb host for backend tests
 *   --backend_keyspace=<keyspace> - sets the cassandra/scylladb keyspace for backend tests
 *   --clean-gcda                  - delete all gcda files defore running tests
 */
int
main(int argc, char* argv[])
{
    util::setTerminationHandler();
    testing::InitGoogleTest(&argc, argv);
    TestGlobals::instance().parse(argc, argv);

    return RUN_ALL_TESTS();
}
