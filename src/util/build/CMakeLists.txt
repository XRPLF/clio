include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/ClioVersion.cmake)

add_library(clio_build_version)
target_sources(clio_build_version PRIVATE Build.cpp)
target_link_libraries(clio_build_version PUBLIC clio_options)
target_compile_definitions(
    clio_build_version
    PRIVATE
        CLIO_VERSION="${CLIO_VERSION}"
        GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
        GIT_BUILD_BRANCH="${GIT_BUILD_BRANCH}"
        BUILD_DATE="${BUILD_DATE}"
)
