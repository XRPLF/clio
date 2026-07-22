#include "util/build/Build.hpp"

#include <string>

namespace util::build {

#ifndef CLIO_VERSION
#error "CLIO_VERSION must be defined"
#endif
#ifndef GIT_COMMIT_HASH
#error "GIT_COMMIT_HASH must be defined"
#endif
#ifndef GIT_BUILD_BRANCH
#error "GIT_BUILD_BRANCH must be defined"
#endif
#ifndef BUILD_DATE
#error "BUILD_DATE must be defined"
#endif

static constexpr char kVersionString[] = CLIO_VERSION;
static constexpr char kGitCommitHash[] = GIT_COMMIT_HASH;
static constexpr char kGitBuildBranch[] = GIT_BUILD_BRANCH;
static constexpr char kBuildDate[] = BUILD_DATE;

std::string const&
getClioVersionString()
{
    static std::string const kValue = kVersionString;
    return kValue;
}

std::string const&
getClioFullVersionString()
{
    static std::string const kValue = "clio-" + getClioVersionString();
    return kValue;
}

std::string const&
getGitCommitHash()
{
    static std::string const kValue = kGitCommitHash;
    return kValue;
}

std::string const&
getGitBuildBranch()
{
    static std::string const kValue = kGitBuildBranch;
    return kValue;
}

std::string const&
getBuildDate()
{
    static std::string const kValue = kBuildDate;
    return kValue;
}

}  // namespace util::build
