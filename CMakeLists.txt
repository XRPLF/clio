cmake_minimum_required(VERSION 3.20)

project(
    clio
    VERSION ${CLIO_VERSION}
    HOMEPAGE_URL "https://github.com/XRPLF/clio"
    DESCRIPTION "An XRP Ledger API Server"
)

# =========================== Options ====================================== #
option(verbose "Verbose build" FALSE)
option(tests "Build unit tests" FALSE)
option(integration_tests "Build integration tests" FALSE)
option(benchmark "Build benchmarks" FALSE)
option(docs "Generate doxygen docs" FALSE)
option(coverage "Build test coverage report" FALSE)
option(package "Create distribution packages" FALSE)
option(lint "Run clang-tidy checks during compilation" FALSE)
option(static "Statically linked Clio" FALSE)
option(snapshot "Build snapshot tool" FALSE)
option(
    time_trace
    "Build using -ftime-trace to create compiler trace reports"
    FALSE
)

# ========================================================================== #
set(san "" CACHE STRING "Add sanitizer instrumentation")
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set_property(CACHE san PROPERTY STRINGS ";undefined;memory;address;thread")
# ========================================================================== #

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Include required modules
include(Ccache)
include(CheckCXXCompilerFlag)
include(ClangTidy)
include(Linker)

add_library(clio_options INTERFACE)
target_compile_features(clio_options INTERFACE cxx_std_23) # Clio needs c++23 but deps can remain c++20 for now
target_include_directories(clio_options INTERFACE ${CMAKE_SOURCE_DIR}/src)

if(verbose)
    set(CMAKE_VERBOSE_MAKEFILE TRUE)
endif()

# Clio tweaks and checks
include(CheckCompiler)
include(Settings)
include(SourceLocation)

# Clio deps
include(deps/libxrpl)
include(deps/Boost)
include(deps/OpenSSL)
include(deps/Threads)
include(deps/libfmt)
include(deps/cassandra)
include(deps/libbacktrace)
include(deps/spdlog)

add_subdirectory(src)
add_subdirectory(tests)

if(benchmark)
    add_subdirectory(benchmarks)
endif()

# Enable selected sanitizer if enabled via `san`
if(san)
    set(SUPPORTED_SANITIZERS "address" "thread" "memory" "undefined")
    if(NOT san IN_LIST SUPPORTED_SANITIZERS)
        message(
            FATAL_ERROR
            "Error: Unsupported sanitizer '${san}'. Supported values are: ${SUPPORTED_SANITIZERS}."
        )
    endif()

    # Sanitizers recommend minimum of -O1 for reasonable performance so we enable it for debug builds
    set(SAN_OPTIMIZATION_FLAG "")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(SAN_OPTIMIZATION_FLAG -O1)
    endif()
    target_compile_options(
        clio_options
        INTERFACE ${SAN_OPTIMIZATION_FLAG} ${SAN_FLAG} -fno-omit-frame-pointer
    )

    target_link_libraries(clio_options INTERFACE ${SAN_FLAG} ${SAN_LIB})
endif()

# Generate `docs` target for doxygen documentation if enabled Note: use `make docs` to generate the documentation
if(docs)
    add_subdirectory(docs)
endif()

include(install/install)
if(package)
    include(ClioPackage)
endif()

if(snapshot)
    add_subdirectory(tools/snapshot)
endif()
