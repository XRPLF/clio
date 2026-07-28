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
option(snapshot "Build snapshot tool" FALSE)
option(
    time_trace
    "Build using -ftime-trace to create compiler trace reports"
    FALSE
)

# ========================================================================== #
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Include required modules
include(Ccache)
include(CheckCXXCompilerFlag)
include(ClangTidy)
include(Linker)

add_library(clio_options INTERFACE)
target_compile_features(clio_options INTERFACE cxx_std_23)
target_include_directories(clio_options INTERFACE ${CMAKE_SOURCE_DIR}/src)

if(
    CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
    OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
)
    # Note: -static-libstdc++ can statically link both libstdc++ and libc++
    target_link_libraries(
        clio_options
        INTERFACE -static-libstdc++ -static-libgcc
    )
endif()

# On aarch64, libatomic is required for atomic operations. It is not needed on x86_64.
# Linking it statically on Linux
if(
    CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64"
    AND CMAKE_SYSTEM_NAME STREQUAL "Linux"
)
    target_link_options(
        clio_options
        INTERFACE -Wl,--push-state -Wl,-Bstatic -latomic -Wl,--pop-state
    )
endif()

# Apply sanitizer instrumentation if requested and define SANITIZERS_ENABLED.
# Must come before the modules and subdirectories below that key off it.
include(Sanitizers)

if(verbose)
    set(CMAKE_VERBOSE_MAKEFILE TRUE)
endif()

# Clio tweaks and checks
include(CheckCompiler)
include(Settings)
include(PatchNixBinary)

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
