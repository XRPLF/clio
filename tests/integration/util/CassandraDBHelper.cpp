#include "util/CassandraDBHelper.hpp"

#include "data/cassandra/Handle.hpp"
#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/Result.hpp"

#include <fmt/format.h>

#include <cstdint>
#include <string>

data::cassandra::ResultOrError
writeTxFromCSVString(
    std::string const& space,
    std::string const& record,
    data::cassandra::Handle const& handler
)
{
    std::string const statement = fmt::format(
        "INSERT INTO {}.transactions (hash, date, ledger_sequence, metadata, transaction) VALUES "
        "({})",
        space,
        record
    );

    return handler.execute(statement);
}

data::cassandra::ResultOrError
writeObjectFromCSVString(
    std::string const& space,
    std::string const& record,
    data::cassandra::Handle const& handler
)
{
    std::string const statement =
        fmt::format("INSERT INTO {}.objects (key, sequence, object) VALUES ({})", space, record);

    return handler.execute(statement);
}

data::cassandra::ResultOrError
writeLedgerFromCSVString(
    std::string const& space,
    std::string const& record,
    data::cassandra::Handle const& handler
)
{
    std::string const statement =
        fmt::format("INSERT INTO {}.ledgers (sequence, header) VALUES ({})", space, record);
    return handler.execute(statement);
}

data::cassandra::ResultOrError
writeLedgerRange(
    std::string const& space,
    std::uint32_t minSeq,
    std::uint32_t maxSeq,
    data::cassandra::Handle const& handler
)
{
    std::string statement = fmt::format(
        "INSERT INTO {}.ledger_range (sequence, is_latest) VALUES ({},false)", space, minSeq
    );
    auto ret = handler.execute(statement);

    if (!ret)
        return ret;

    statement = fmt::format(
        "INSERT INTO {}.ledger_range (sequence, is_latest) VALUES ({},true)", space, maxSeq
    );
    return handler.execute(statement);
}
