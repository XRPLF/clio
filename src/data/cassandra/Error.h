//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <cassandra.h>

#include <string>
#include <utility>

namespace data::cassandra {

/**
 * @brief A simple container for both error message and error code.
 */
class CassandraError
{
    std::string message_;
    uint32_t code_{};

public:
    CassandraError() = default;  // default constructible required by Expected
    CassandraError(std::string message, uint32_t code) : message_{std::move(message)}, code_{code}
    {
    }

    template <typename T>
    friend std::string
    operator+(T const& lhs, CassandraError const& rhs) requires std::is_convertible_v<T, std::string>
    {
        return lhs + rhs.message();
    }

    template <typename T>
    friend bool
    operator==(T const& lhs, CassandraError const& rhs) requires std::is_convertible_v<T, std::string>
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

    /**
     * @return The final error message as a std::string
     */
    std::string
    message() const
    {
        return message_;
    }

    /**
     * @return The error code
     */
    uint32_t
    code() const
    {
        return code_;
    }

    /**
     * @return true if the wrapped error is considered a timeout; false otherwise
     */
    bool
    isTimeout() const
    {
        return code_ == CASS_ERROR_LIB_NO_HOSTS_AVAILABLE or code_ == CASS_ERROR_LIB_REQUEST_TIMED_OUT or
            code_ == CASS_ERROR_SERVER_UNAVAILABLE or code_ == CASS_ERROR_SERVER_OVERLOADED or
            code_ == CASS_ERROR_SERVER_READ_TIMEOUT;
    }

    /**
     * @return true if the wrapped error is an invalid query; false otherwise
     */
    bool
    isInvalidQuery() const
    {
        return code_ == CASS_ERROR_SERVER_INVALID_QUERY;
    }
};

}  // namespace data::cassandra
