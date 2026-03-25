#pragma once

#include <boost/system/detail/error_code.hpp>

namespace web::ng {

/**
 * @brief Error of any async operation.
 */
using Error = boost::system::error_code;

}  // namespace web::ng
