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
     *
     * @tparam T Type of the value to write
     * @param value The value to write
     * @return The stream
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
