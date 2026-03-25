#pragma once

#include <string>

namespace util::build {

std::string const&
getClioVersionString();

std::string const&
getClioFullVersionString();

std::string const&
getGitCommitHash();

std::string const&
getGitBuildBranch();

std::string const&
getBuildDate();

}  // namespace util::build
