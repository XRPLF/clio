#ifndef RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED
#define RIPPLE_APP_REPORTING_BACKENDFACTORY_H_INCLUDED

#include <boost/algorithm/string.hpp>
#include <backend/BackendInterface.h>
#include <backend/CassandraBackend.h>

namespace Backend {
std::shared_ptr<BackendInterface>
make_Backend(boost::asio::io_context& ioc, boost::json::object const& config)
{
    BOOST_LOG_TRIVIAL(info) << __func__ << ": Constructing BackendInterface";

    boost::json::object dbConfig = config.at("database").as_object();

    bool readOnly = false;
    if (config.contains("read_only"))
        readOnly = config.at("read_only").as_bool();

    auto type = dbConfig.at("type").as_string();

    std::shared_ptr<BackendInterface> backend = nullptr;

    if (boost::iequals(type, "cassandra"))
    {
        if (config.contains("online_delete"))
            dbConfig.at(type).as_object()["ttl"] =
                config.at("online_delete").as_int64() * 4;
        backend = std::make_shared<CassandraBackend>(
            ioc, dbConfig.at(type).as_object());
    }

    if (!backend)
        throw std::runtime_error("Invalid database type");

    backend->open(readOnly);
    auto rng = backend->hardFetchLedgerRangeNoThrow();
    if (rng)
    {
        backend->updateRange(rng->minSequence);
        backend->updateRange(rng->maxSequence);
    }

    BOOST_LOG_TRIVIAL(info)
        << __func__ << ": Constructed BackendInterface Successfully";

    return backend;
}
}  // namespace Backend

#endif  // RIPPLE_REPORTING_BACKEND_FACTORY
