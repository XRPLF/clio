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
  set(GIT_COMMAND show -s --date=format:%Y%m%d%H%M%S --format=%cd)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} ${GIT_COMMAND} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  set(CLIO_VERSION "${DATE}-${BRANCH}-${REV}")
  set(DOC_CLIO_VERSION "develop")
else ()
  set(GIT_COMMAND describe --tags)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} ${GIT_COMMAND} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE CLIO_TAG_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  set(CLIO_VERSION "${CLIO_TAG_VERSION}")
  set(DOC_CLIO_VERSION "${CLIO_TAG_VERSION}")
endif ()

if (CMAKE_BUILD_TYPE MATCHES Debug)
  set(CLIO_VERSION "${CLIO_VERSION}+DEBUG")
endif ()

message(STATUS "Build version: ${CLIO_VERSION}")

configure_file(${CMAKE_CURRENT_LIST_DIR}/Build.cpp.in ${CMAKE_CURRENT_LIST_DIR}/../src/util/build/Build.cpp)
