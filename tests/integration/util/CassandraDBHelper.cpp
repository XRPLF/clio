//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022-2024, the clio developers.

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

#include "data/cassandra/Handle.hpp"
#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/Result.hpp"

#include <fmt/core.h>

#include <cstdint>
#include <string>

data::cassandra::ResultOrError
writeTxFromCSVString(std::string const& space, std::string const& record, data::cassandra::Handle const& handler)
{
    std::string const statement = fmt::format(
        "INSERT INTO {}.transactions (hash, date, ledger_sequence, metadata, transaction) VALUES ({})", space, record
    );

    return handler.execute(statement);
}

data::cassandra::ResultOrError
writeObjectFromCSVString(std::string const& space, std::string const& record, data::cassandra::Handle const& handler)
{
    std::string const statement =
        fmt::format("INSERT INTO {}.objects (key, sequence, object) VALUES ({})", space, record);

    return handler.execute(statement);
}

data::cassandra::ResultOrError
writeLedgerFromCSVString(std::string const& space, std::string const& record, data::cassandra::Handle const& handler)
{
    std::string const statement = fmt::format("INSERT INTO {}.ledgers (sequence, header) VALUES ({})", space, record);
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
    std::string statement =
        fmt::format("INSERT INTO {}.ledger_range (sequence, is_latest) VALUES ({},false)", space, minSeq);
    auto ret = handler.execute(statement);

    if (!ret)
        return ret;

    statement = fmt::format("INSERT INTO {}.ledger_range (sequence, is_latest) VALUES ({},true)", space, maxSeq);
    return handler.execute(statement);
}
