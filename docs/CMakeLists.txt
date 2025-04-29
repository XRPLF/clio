cmake_minimum_required(VERSION 3.20)
project(docs)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/ClioVersion.cmake)

# cmake-format: off
# Generate `docs` target for doxygen documentation
# Note: use `cmake --build . --target docs` from your `build` directory to generate the documentation
# cmake-format: on
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Docs.cmake)
