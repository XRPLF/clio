#ifndef RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED
#include <boost/algorithm/string.hpp>
#include <reporting/BackendInterface.h>
#include <reporting/CassandraBackend.h>
#include <reporting/PostgresBackend.h>
namespace Backend {
std::unique_ptr<BackendInterface>
makeBackend(boost::json::object const& config)
{
    boost::json::object const& dbConfig = config.at("database").as_object();

    auto type = dbConfig.at("type").as_string();

    if (boost::iequals(type, "cassandra"))
    {
        auto backend =
            std::make_unique<CassandraBackend>(dbConfig.at(type).as_object());
        return std::move(backend);
    }
    else if (boost::iequals(type, "postgres"))
    {
        auto backend =
            std::make_unique<PostgresBackend>(dbConfig.at(type).as_object());
        return std::move(backend);
    }
    return nullptr;
}
}  // namespace Backend
#endif
