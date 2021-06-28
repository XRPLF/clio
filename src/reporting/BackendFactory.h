#ifndef RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED

#include <boost/algorithm/string.hpp>
#include <reporting/BackendInterface.h>
#include <reporting/CassandraBackend.h>
#include <reporting/PostgresBackend.h>

namespace Backend {
std::shared_ptr<BackendInterface>
make_Backend(boost::json::object const& config)
{
    BOOST_LOG_TRIVIAL(info) << __func__ << ": Constructing BackendInterface";

    boost::json::object const& dbConfig = config.at("database").as_object();

    bool readOnly = false;
    if (config.contains("read_only"))
        readOnly = config.at("read_only").as_bool();

    auto type = dbConfig.at("type").as_string();

    std::shared_ptr<BackendInterface> backend = nullptr;

    if (boost::iequals(type, "cassandra"))
    {
        backend =
            std::make_shared<CassandraBackend>(dbConfig.at(type).as_object());
    }
    else if (boost::iequals(type, "postgres"))
    {
        backend =
            std::make_shared<PostgresBackend>(dbConfig.at(type).as_object());
    }

    if (!backend)
        throw std::runtime_error("Invalid database type");

    backend->open(readOnly);

    BOOST_LOG_TRIVIAL(info) << __func__
                            << ": Constructed BackendInterface Successfully";

    return backend;
}
}  // namespace Backend

#endif //RIPPLE_REPORTING_BACKEND_FACTORY
