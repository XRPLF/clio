cmake_minimum_required(VERSION 3.16.3)

set(CMAKE_PROJECT_INCLUDE_BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ClioVersion.cmake)

project(clio VERSION ${CLIO_VERSION} HOMEPAGE_URL "https://github.com/XRPLF/clio"
        DESCRIPTION "An XRP Ledger API Server"
)

# =========================== Options ====================================== #
option(verbose "Verbose build" FALSE)
option(tests "Build tests" FALSE)
option(benchmark "Build benchmarks" FALSE)
option(docs "Generate doxygen docs" FALSE)
option(coverage "Build test coverage report" FALSE)
option(packaging "Create distribution packages" FALSE)
option(lint "Run clang-tidy checks during compilation" FALSE)
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

add_library(clio_options INTERFACE)
target_compile_features(clio_options INTERFACE cxx_std_23) # Clio needs c++23 but deps can remain c++20 for now
target_include_directories(clio_options INTERFACE ${CMAKE_SOURCE_DIR}/src)

# Set coverage build options
if (coverage)
  if (NOT tests)
    message(FATAL_ERROR "Coverage requires tests to be enabled")
  endif ()
  include(CodeCoverage)
  append_coverage_compiler_flags_to_target(clio_options INTERFACE)
endif ()

if (verbose)
  set(CMAKE_VERBOSE_MAKEFILE TRUE)
endif ()

if (packaging)
  add_definitions(-DPKG=1)
  target_compile_definitions(clio_options INTERFACE PKG=1)
endif ()

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

add_subdirectory(src)

if (tests)
  add_subdirectory(unittests)
endif ()

if (benchmark)
  add_subdirectory(benchmarks)
endif ()

# Enable selected sanitizer if enabled via `san`
if (san)
  target_compile_options(
    clio PUBLIC # Sanitizers recommend minimum of -O1 for reasonable performance
                $<$<CONFIG:Debug>:-O1> ${SAN_FLAG} -fno-omit-frame-pointer
  )
  target_compile_definitions(
    clio PUBLIC $<$<STREQUAL:${san},address>:SANITIZER=ASAN> $<$<STREQUAL:${san},thread>:SANITIZER=TSAN>
                $<$<STREQUAL:${san},memory>:SANITIZER=MSAN> $<$<STREQUAL:${san},undefined>:SANITIZER=UBSAN>
  )
  target_link_libraries(clio INTERFACE ${SAN_FLAG} ${SAN_LIB})
endif ()

# Generate `docs` target for doxygen documentation if enabled Note: use `make docs` to generate the documentation
if (docs)
  add_subdirectory(docs)
endif ()

include(install/install)
if (packaging)
  include(cmake/packaging.cmake) # This file exists only in build runner
endif ()
