#pragma once

#include <string>

/**
 * @brief Build-time information about the Clio binary (version, git commit and branch, build date)
 */
namespace util::build {

/**
 * @brief Get the Clio version
 *
 * @return The version string as provided at build time via CLIO_VERSION
 */
std::string const&
getClioVersionString();

/**
 * @brief Get the full Clio version
 *
 * @return The version string prefixed with "clio-"
 */
std::string const&
getClioFullVersionString();

/**
 * @brief Get the git commit hash Clio was built from
 *
 * @return The commit hash as provided at build time via GIT_COMMIT_HASH
 */
std::string const&
getGitCommitHash();

/**
 * @brief Get the git branch Clio was built from
 *
 * @return The branch name as provided at build time via GIT_BUILD_BRANCH
 */
std::string const&
getGitBuildBranch();

/**
 * @brief Get the date Clio was built
 *
 * @return The build date as provided at build time via BUILD_DATE
 */
std::string const&
getBuildDate();

}  // namespace util::build
