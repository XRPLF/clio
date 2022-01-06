set(RIPPLED_REPO "https://github.com/cjcobb23/rippled.git")
set(RIPPLED_BRANCH "clio")
set(NIH_CACHE_ROOT "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL "")
message(STATUS "Cloning ${RIPPLED_REPO} branch ${RIPPLED_BRANCH}")
FetchContent_Declare(rippled
  GIT_REPOSITORY "${RIPPLED_REPO}"
  GIT_TAG "${RIPPLED_BRANCH}"
  GIT_SHALLOW ON
)

FetchContent_GetProperties(rippled)
if(NOT rippled_POPULATED)
  FetchContent_Populate(rippled)
  add_subdirectory(${rippled_SOURCE_DIR} ${rippled_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(clio PUBLIC xrpl_core grpc_pbufs)
target_include_directories(clio PUBLIC ${rippled_SOURCE_DIR}/src ) # TODO: Seems like this shouldn't be needed?
