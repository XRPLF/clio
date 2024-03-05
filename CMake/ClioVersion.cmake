#[===================================================================[
   write version to source
#]===================================================================]

find_package(Git REQUIRED)

set(GIT_COMMAND rev-parse --short HEAD)
execute_process(
  COMMAND ${GIT_EXECUTABLE} ${GIT_COMMAND} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE REV
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(GIT_COMMAND branch --show-current)
execute_process(
  COMMAND ${GIT_EXECUTABLE} ${GIT_COMMAND} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (BRANCH STREQUAL "")
  set(BRANCH "dev")
endif ()

if (NOT (BRANCH MATCHES master OR BRANCH MATCHES release/*)) # for develop and any other branch name
                                                             # YYYYMMDDHMS-<branch>-<git-rev>
  execute_process(COMMAND date +%Y%m%d%H%M%S OUTPUT_VARIABLE DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
  set(VERSION "${DATE}-${BRANCH}-${REV}")
  set(DOC_CLIO_VERSION "develop")
else ()
  set(GIT_COMMAND describe --tags)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} ${GIT_COMMAND} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE TAG_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  set(VERSION "${TAG_VERSION}-${REV}")
  set(DOC_CLIO_VERSION "${TAG_VERSION}")
endif ()

if (CMAKE_BUILD_TYPE MATCHES Debug)
  set(VERSION "${VERSION}+DEBUG")
endif ()

message(STATUS "Build version: ${VERSION}")
set(clio_version "${VERSION}")

configure_file(${CMAKE_CURRENT_LIST_DIR}/Build.cpp.in ${CMAKE_CURRENT_LIST_DIR}/../src/main/impl/Build.cpp)
