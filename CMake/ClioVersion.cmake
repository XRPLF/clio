#[===================================================================[
   write version to source
#]===================================================================]

find_package(Git REQUIRED)

set(GIT_COMMAND rev-parse --short HEAD)
execute_process(COMMAND ${GIT_EXECUTABLE} ${GIT_COMMAND} OUTPUT_VARIABLE REV OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT (BRANCH MATCHES master OR BRANCH MATCHES release/*)) # for develop and any other branch name YYYYMMDDHMS-<branch>-<git-ref>
  set(GIT_COMMAND branch --show-current)
  execute_process(COMMAND ${GIT_EXECUTABLE} ${GIT_COMMAND} OUTPUT_VARIABLE BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND date +%Y%m%d%H%M%S OUTPUT_VARIABLE DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(VERSION "${DATE}-${BRANCH}-${REV}")
else()
  set(GIT_COMMAND describe --tags)
  execute_process(COMMAND ${GIT_EXECUTABLE} ${GIT_COMMAND} OUTPUT_VARIABLE TAG_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(VERSION "${TAG_VERSION}-${REV}")
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
  set(VERSION "${VERSION}+DEBUG")
endif()

message(STATUS "Build version: ${VERSION}")
set(clio_version "${VERSION}")

configure_file(CMake/Build.cpp.in ${CMAKE_SOURCE_DIR}/src/main/impl/Build.cpp)
