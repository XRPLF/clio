#include <ripple/beast/core/SemanticVersion.h>
#include <boost/preprocessor/stringize.hpp>
#include <algorithm>
#include <main/Build.h>
#include <optional>
#include <stdexcept>

namespace Build {

//--------------------------------------------------------------------------
//  The build version number. You must edit this for each release
//  and follow the format described at http://semver.org/
//------------------------------------------------------------------------------
// clang-format off
char const* const versionString = "1.0.3"
// clang-format on

#if defined(DEBUG) || defined(SANITIZER) || defined(RELEASE)
    "+"
#ifdef CLIO_GIT_COMMIT_HASH
    CLIO_GIT_COMMIT_HASH
    "."
#endif
#ifdef DEBUG
    "DEBUG"
#ifdef SANITIZER
    "."
#endif
#endif

#ifdef SANITIZER
    BOOST_PP_STRINGIZE(SANITIZER)
#endif
#endif
#ifdef PKG
        "-release"
#endif

#define VAL(CLIO_GIT_COMMIT_HASH) #CLIO_GIT_COMMIT_HASH
#define TOSTRING(CLIO_GIT_COMMIT_HASH) VAL(CLIO_GIT_COMMIT_HASH)

    //--------------------------------------------------------------------------
    ;

std::string const&
getClioVersionString()
{
    static std::string const value = [] {
        std::string const s = versionString;
        std::string const t = std::to_string(CLIO_GIT_COMMIT_HASH);
        beast::SemanticVersion v;
        if (!v.parse(s) || v.print() != s)
            throw std::runtime_error(s + ": Bad server version string");
        std::string const result = s + ", Git Commit Hash: " + t;
        return result;
    }();
    return value;
}

std::string const&
getClioFullVersionString()
{
    static std::string const value = "clio-" + getClioVersionString();
    return value;
}

}  // namespace Build
