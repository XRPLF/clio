#pragma once

#include "util/TmpFile.hpp"

#include <string_view>

namespace tests {

std::string_view
sslCert();

TmpFile
sslCertFile();

std::string_view
sslKey();

TmpFile
sslKeyFile();

}  // namespace tests
