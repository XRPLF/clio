#[===================================================================[
   write version to source
#]===================================================================]

find_package(Git REQUIRED)

execute_process(COMMAND ${GIT_EXECUTABLE} branch --show-current OUTPUT_VARIABLE BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE)

set(GIT_COMMAND describe --tags)

execute_process(COMMAND ${GIT_EXECUTABLE} ${GIT_COMMAND} OUTPUT_VARIABLE VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)

set(VERSION "${VERSION}")

if(NOT (BRANCH MATCHES master OR BRANCH MATCHES release/*)) # for develop and any other branch name YYYYMMDDHMS-<git-ref>
  set(GIT_COMMAND rev-parse --short HEAD)
  execute_process(COMMAND date +%Y%m%d%H%M%S OUTPUT_VARIABLE DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(VERSION "${VERSION}-${DATE}")
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(VERSION "${VERSION}+DEBUG")
endif()

message(STATUS "Build version: ${VERSION}")
set(clio_version "${VERSION}")

configure_file(CMake/Build.cpp.in ${CMAKE_SOURCE_DIR}/src/main/impl/Build.cpp)
