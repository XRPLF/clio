#ifndef CLIO_BUILD_INFO_H
#define CLIO_BUILD_INFO_H

#include <string>

namespace Build {

std::string const&
getClioVersionString();

std::string const&
getClioFullVersionString();

}  // namespace Build

#endif  // CLIO_BUILD_INFO_H
