cmake_minimum_required(VERSION 3.20)
project(docs)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/ClioVersion.cmake)

# Generate `docs` target for doxygen documentation
# Note: use `cmake --build . --target docs` from your `build` directory to generate the documentation
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Docs.cmake)
