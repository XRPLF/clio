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

#include "util/TestGlobals.h"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>

TestGlobals&
TestGlobals::instance()
{
    static TestGlobals inst;
    return inst;
}

void
TestGlobals::parse(int argc, char* argv[])
{
    namespace po = boost::program_options;

    // clang-format off
    po::options_description description("Clio UT options");
    description.add_options()
        ("backend_host", po::value<std::string>()->default_value(TestGlobals::backendHost), 
            "sets the cassandra/scylladb host for backend tests")
        ("backend_keyspace", po::value<std::string>()->default_value(TestGlobals::backendKeyspace), 
            "sets the cassandra/scylladb keyspace for backend tests")
    ;
    // clang-format on

    po::variables_map parsed;
    po::store(po::command_line_parser(argc, argv).options(description).run(), parsed);
    po::notify(parsed);

    backendHost = parsed["backend_host"].as<std::string>();
    backendKeyspace = parsed["backend_keyspace"].as<std::string>();
}
