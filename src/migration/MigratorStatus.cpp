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
    return kStatusStrMap[static_cast<size_t>(status_)];
}

MigratorStatus
MigratorStatus::fromString(std::string const& statusStr)
{
    for (std::size_t i = 0; i < kStatusStrMap.size(); ++i) {
        if (statusStr == kStatusStrMap[i]) {
            return MigratorStatus(static_cast<Status>(i));
        }
    }
    return MigratorStatus(Status::NotMigrated);
}

}  // namespace migration
