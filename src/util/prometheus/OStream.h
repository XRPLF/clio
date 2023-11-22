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

#include <boost/iostreams/filtering_stream.hpp>

#include <string>

namespace util::prometheus {

/**
 * @brief A stream that can optionally compress its data
 */
class OStream {
public:
    /**
     * @brief Construct a new OStream object
     *
     * @param compressionEnabled Whether to compress the data
     */
    OStream(bool compressionEnabled);

    OStream(OStream const&) = delete;
    OStream(OStream&&) = delete;
    ~OStream() = default;

    /**
     * @brief Write to the stream
     */
    template <typename T>
    OStream&
    operator<<(T const& value)
    {
        stream_ << value;
        return *this;
    }

    /**
     * @brief Get the data from the stream.
     *
     * @note This resets the stream and clears the buffer. Stream cannot be used after this.
     *
     * @return The data
     */
    std::string
    data() &&;

private:
    bool compressionEnabled_;
    std::string buffer_;
    boost::iostreams::filtering_ostream stream_;
};

}  // namespace util::prometheus
