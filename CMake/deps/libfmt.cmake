FetchContent_Declare(
  libfmt
  URL https://github.com/fmtlib/fmt/releases/download/9.1.0/fmt-9.1.0.zip
)

FetchContent_GetProperties(libfmt)

if(NOT libfmt_POPULATED)
  FetchContent_Populate(libfmt)
  add_subdirectory(${libfmt_SOURCE_DIR} ${libfmt_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(clio PUBLIC fmt)

