set(CLIO_REPO "https://github.com/XRPLF/clio.git")
set(CLIO_BRANCH "1.0.4")

add_library(clio STATIC IMPORTED GLOBAL)
add_library(xrpl_core STATIC IMPORTED GLOBAL)

ExternalProject_Add(clio_src
  GIT_REPOSITORY "${CLIO_REPO}"
  GIT_TAG "${CLIO_BRANCH}"
  GIT_SHALLOW ON
  INSTALL_COMMAND ""
  CMAKE_ARGS
  -DBUILD_TESTS=OFF
  -DPACKAGING=OFF
  )

ExternalProject_Get_Property(clio_src SOURCE_DIR)
ExternalProject_Get_Property(clio_src BINARY_DIR)

file(MAKE_DIRECTORY ${SOURCE_DIR}/src)
set_target_properties(clio PROPERTIES
  IMPORTED_LOCATION
  ${BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}clio_static.a
  INTERFACE_INCLUDE_DIRECTORIES
  ${SOURCE_DIR}/src
  )

file(MAKE_DIRECTORY ${BINARY_DIR}/_deps/rippled-build)
file(MAKE_DIRECTORY ${BINARY_DIR}/_deps/rippled-src/src)
set_target_properties(xrpl_core PROPERTIES
  IMPORTED_LOCATION
  ${BINARY_DIR}/_deps/rippled-build/${CMAKE_STATIC_LIBRARY_PREFIX}xrpl_core.a
  INTERFACE_INCLUDE_DIRECTORIES
  ${BINARY_DIR}/_deps/rippled-src/src
  )

add_dependencies(clio clio_src)
add_dependencies(xrpl_core clio_src)

add_library(date STATIC IMPORTED GLOBAL)
file(MAKE_DIRECTORY
  ${BINARY_DIR}/unix_makefiles/AppleClang_14.0.0.14000029/Release/hh_date_src-src/include)
set_target_properties(date PROPERTIES
  IMPORTED_LOCATION
  ${BINARY_DIR}/unix_makefiles/AppleClang_14.0.0.14000029/Release/hh_date_src-src/include/date/date.h
  INTERFACE_INCLUDE_DIRECTORIES
  ${BINARY_DIR}/unix_makefiles/AppleClang_14.0.0.14000029/Release/hh_date_src-src/include
  )
