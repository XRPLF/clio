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

#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace migration {

/**
 * @brief The status of a migrator, it provides the helper functions to convert the status to string and vice versa
 */
class MigratorStatus {
public:
    /**
     * @brief The status of a migrator
     */
    enum Status { Migrated, NotMigrated, NotKnown, NumStatuses };

    /**
     * @brief Construct a new Migrator Status object with the given status
     *
     * @param status The status of the migrator
     */
    MigratorStatus(Status status) : status_(status)
    {
    }

    /**
     * @brief Compare the status with another MigratorStatus
     *
     * @param other The other status to compare
     * @return true if the status is equal to the other status, false otherwise
     */
    bool
    operator==(MigratorStatus const& other) const;

    /**
     * @brief Compare the status with another status
     * @param other The other status to compare
     * @return true if the status is equal to the other status, false otherwise
     */
    bool
    operator==(Status const& other) const;

    /**
     * @brief Convert the status to string
     *
     * @return The string representation of the status
     */
    std::string
    toString() const;

    /**
     * @brief Convert the string to status
     *
     * @param statusStr The string to convert
     * @return The status representation of the string
     */
    static MigratorStatus
    fromString(std::string const& statusStr);

private:
    static constexpr std::array<char const*, static_cast<size_t>(NumStatuses)> statusStrMap = {
        "Migrated",
        "NotMigrated",
        "NotKnown"
    };

    Status status_;
};
}  // namespace migration
