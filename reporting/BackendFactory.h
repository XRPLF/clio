#ifndef RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED
#include <reporting/BackendInterface.h>
#include <reporting/CassandraBackend.h>
namespace Backend {
std::unique_ptr<BackendInterface>
makeBackend(boost::json::object const& config)
{
    boost::json::object const& dbConfig = config.at("database").as_object();

    if (dbConfig.contains("cassandra"))
    {
        auto backend = std::make_unique<CassandraBackend>(
            dbConfig.at("cassandra").as_object());
        return std::move(backend);
    }
    return nullptr;
}
}  // namespace Backend
#endif
