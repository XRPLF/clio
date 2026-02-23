//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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
    static std::string const value = kVERSION_STRING;  // NOLINT(readability-identifier-naming)
    return value;
}

std::string const&
getClioFullVersionString()
{
    static std::string const kVALUE =
        "clio-" + getClioVersionString();  // NOLINT(readability-identifier-naming)
    return kVALUE;
}

std::string const&
getGitCommitHash()
{
    static std::string const value = kGIT_COMMIT_HASH;  // NOLINT(readability-identifier-naming)
    return value;
}

std::string const&
getGitBuildBranch()
{
    static std::string const value = kGIT_BUILD_BRANCH;  // NOLINT(readability-identifier-naming)
    return value;
}

std::string const&
getBuildDate()
{
    static std::string const value = kBUILD_DATE;  // NOLINT(readability-identifier-naming)
    return value;
}

}  // namespace util::build
