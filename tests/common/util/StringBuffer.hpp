#pragma once

#include <sstream>
#include <string>

/**
 * @brief A simple string buffer that can be used to mock std::cout for console logging.
 */
class StringBuffer final : public std::stringbuf {
public:
    std::string
    getStrAndReset()
    {
        auto value = str();
        str("");
        return value;
    }
};
