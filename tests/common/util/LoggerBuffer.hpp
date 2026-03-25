#pragma once
#include "util/StringBuffer.hpp"

#include <ostream>
#include <string>

class LoggerBuffer {
public:
    std::string
    getStrAndReset()
    {
        return buffer_.getStrAndReset();
    }

    std::ostream&
    getStream()
    {
        return stream_;
    }

private:
    StringBuffer buffer_;
    std::ostream stream_ = std::ostream{&buffer_};
};
