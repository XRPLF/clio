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

#include "migration/MigratiorStatus.hpp"

#include <cstddef>
#include <string>

namespace migration {

bool
MigratorStatus::operator==(MigratorStatus const& other) const
{
    return status_ == other.status_;
}

bool
MigratorStatus::operator==(Status const& other) const
{
    return status_ == other;
}

std::string
MigratorStatus::toString() const
{
    return kSTATUS_STR_MAP[static_cast<size_t>(status_)];
}

MigratorStatus
MigratorStatus::fromString(std::string const& statusStr)
{
    for (std::size_t i = 0; i < kSTATUS_STR_MAP.size(); ++i) {
        if (statusStr == kSTATUS_STR_MAP[i]) {
            return MigratorStatus(static_cast<Status>(i));
        }
    }
    return MigratorStatus(Status::NotMigrated);
}

}  // namespace migration
