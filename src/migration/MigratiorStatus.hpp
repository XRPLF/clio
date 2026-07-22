#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace migration {

/**
 * @brief The status of a migrator, it provides the helper functions to convert the status to string
 * and vice versa
 */
class MigratorStatus {
public:
    /**
     * @brief The status of a migrator
     */
    enum class Status { Migrated, NotMigrated, NotKnown, NumStatuses };

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
    [[nodiscard]] std::string
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
    static constexpr std::array<char const*, static_cast<size_t>(Status::NumStatuses)>
        kStatusStrMap = {"Migrated", "NotMigrated", "NotKnown"};

    Status status_;
};
}  // namespace migration
