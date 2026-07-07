#pragma once

#include <cassandra.h>

#include <concepts>
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace data::cassandra::impl {

/**
 * @brief Throw a std::logic_error if the given driver return code is not CASS_OK.
 *
 * @param rc The driver return code.
 * @param label A label describing the failed operation, included in the error message.
 */
inline void
throwErrorIfNeeded(CassError const rc, std::string_view const label)
{
    if (rc != CASS_OK)
        throw std::logic_error('[' + std::string{label} + "]: " + cass_error_desc(rc));
}

}  // namespace data::cassandra::impl

namespace data::cassandra {

/**
 * @brief A simple container for both error message and error code.
 */
class CassandraError {
    std::string message_;
    uint32_t code_{};

public:
    // default constructible required by Expected
    CassandraError() = default;

    /**
     * @brief Construct a new CassandraError object
     *
     * @param message The error message
     * @param code The error code
     */
    CassandraError(std::string message, uint32_t code) : message_{std::move(message)}, code_{code}
    {
    }

    /** @cond */
    template <typename T>
    friend std::string
    operator+(T const& lhs, CassandraError const& rhs)
        requires std::is_convertible_v<T, std::string>
    {
        return lhs + rhs.message();
    }

    template <typename T>
    friend bool
    operator==(T const& lhs, CassandraError const& rhs)
        requires std::is_convertible_v<T, std::string>
    {
        return lhs == rhs.message();
    }

    template <std::integral T>
    friend bool
    operator==(T const& lhs, CassandraError const& rhs)
    {
        return lhs == rhs.code();
    }

    friend std::ostream&
    operator<<(std::ostream& os, CassandraError const& err)
    {
        os << err.message();
        return os;
    }
    /** @endcond */

    /**
     * @return The final error message as a std::string
     */
    [[nodiscard]] std::string
    message() const
    {
        return message_;
    }

    /**
     * @return The error code
     */
    [[nodiscard]] uint32_t
    code() const
    {
        return code_;
    }

    /**
     * @return true if the wrapped error is considered a timeout; false otherwise
     */
    [[nodiscard]] bool
    isTimeout() const
    {
        return code_ == CASS_ERROR_LIB_NO_HOSTS_AVAILABLE or
            code_ == CASS_ERROR_LIB_REQUEST_TIMED_OUT or code_ == CASS_ERROR_SERVER_UNAVAILABLE or
            code_ == CASS_ERROR_SERVER_OVERLOADED or code_ == CASS_ERROR_SERVER_READ_TIMEOUT;
    }

    /**
     * @return true if the wrapped error is an invalid query; false otherwise
     */
    [[nodiscard]] bool
    isInvalidQuery() const
    {
        return code_ == CASS_ERROR_SERVER_INVALID_QUERY;
    }
};

}  // namespace data::cassandra
