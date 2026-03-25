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

static constexpr char kVERSION_STRING[] = CLIO_VERSION;
static constexpr char kGIT_COMMIT_HASH[] = GIT_COMMIT_HASH;
static constexpr char kGIT_BUILD_BRANCH[] = GIT_BUILD_BRANCH;
static constexpr char kBUILD_DATE[] = BUILD_DATE;

std::string const&
getClioVersionString()
{
    static std::string const kVALUE = kVERSION_STRING;
    return kVALUE;
}

std::string const&
getClioFullVersionString()
{
    static std::string const kVALUE = "clio-" + getClioVersionString();
    return kVALUE;
}

std::string const&
getGitCommitHash()
{
    static std::string const kVALUE = kGIT_COMMIT_HASH;
    return kVALUE;
}

std::string const&
getGitBuildBranch()
{
    static std::string const kVALUE = kGIT_BUILD_BRANCH;
    return kVALUE;
}

std::string const&
getBuildDate()
{
    static std::string const kVALUE = kBUILD_DATE;
    return kVALUE;
}

}  // namespace util::build
