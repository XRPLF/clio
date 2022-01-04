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

# set(rippled_SOURCE_DIR ${rippled_SOURCE_DIR} PARENT_SCOPE)
