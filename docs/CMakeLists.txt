cmake_minimum_required(VERSION 3.16.3)
project(docs)

include(${CMAKE_CURRENT_SOURCE_DIR}/../CMake/ClioVersion.cmake)

# Generate `docs` target for doxygen documentation if enabled Note: use `make docs` to generate the documentation
include(${CMAKE_CURRENT_SOURCE_DIR}/../CMake/Docs.cmake)
