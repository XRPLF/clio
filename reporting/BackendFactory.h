#ifndef RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED

#include <boost/algorithm/string.hpp>
#include <reporting/BackendInterface.h>
#include <reporting/CassandraBackend.h>
#include <reporting/PostgresBackend.h>

namespace Backend {
std::unique_ptr<BackendInterface>
make_Backend(boost::json::object const& config)
{
    BOOST_LOG_TRIVIAL(info) << __func__ << ": Constructing BackendInterface";

    boost::json::object dbConfig = config.at("database").as_object();

    bool readOnly = false;
    if (config.contains("read_only"))
        readOnly = config.at("read_only").as_bool();

    auto type = dbConfig.at("type").as_string();

    std::unique_ptr<BackendInterface> backend = nullptr;

    if (boost::iequals(type, "cassandra"))
    {
        if (config.contains("online_delete"))
            dbConfig.at(type).as_object()["ttl"] =
                config.at("online_delete").as_int64() * 4;
        backend =
            std::make_unique<CassandraBackend>(dbConfig.at(type).as_object());
    }
    else if (boost::iequals(type, "postgres"))
    {
        backend =
            std::make_unique<PostgresBackend>(dbConfig.at(type).as_object());
    }

    if (!backend)
        throw std::runtime_error("Invalid database type");

    backend->open(readOnly);
    backend->checkFlagLedgers();

    BOOST_LOG_TRIVIAL(info)
        << __func__ << ": Constructed BackendInterface Successfully";

    return backend;
}
}  // namespace Backend

#endif  // RIPPLE_REPORTING_BACKEND_FACTORY
