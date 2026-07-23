#include <TestGlobals.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>

#include <string>

TestGlobals&
TestGlobals::instance()
{
    static TestGlobals kInst;
    return kInst;
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
