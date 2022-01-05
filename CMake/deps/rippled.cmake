set(NIH_CACHE_ROOT "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL "")
FetchContent_Declare(rippled
  GIT_REPOSITORY https://github.com/cjcobb23/rippled.git
  GIT_TAG clio
  GIT_SHALLOW ON
)

FetchContent_GetProperties(rippled)
if(NOT rippled_POPULATED)
  FetchContent_Populate(rippled)
  add_subdirectory(${rippled_SOURCE_DIR} ${rippled_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(clio PUBLIC xrpl_core grpc_pbufs)
target_include_directories(clio PUBLIC ${rippled_SOURCE_DIR}/src ) # TODO: Seems like this shouldn't be needed?
