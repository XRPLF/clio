add_executable(clio_dbtests)

target_sources(
  clio_dbtests
  PRIVATE BackendFactoryTests.cpp
          cassandra/BackendTests.cpp
          cassandra/BaseTests.cpp
          # Utils
          ../unittests/util/StringUtils.cpp
          ../unittests/util/TestGlobals.cpp
          # Test runner
          Main.cpp
)

include(deps/gtest)

# Fix for dwarf5 bug on ci. IS STILL NEEDED???
target_compile_options(clio_options INTERFACE -gdwarf-4)

target_include_directories(clio_dbtests PRIVATE . ../unittests)
target_link_libraries(clio_dbtests PUBLIC clio gtest::gtest)
set_target_properties(clio_dbtests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
