add_executable(clio_dbtests)

target_sources(
  clio_dbtests
  PRIVATE data/BackendFactoryTests.cpp
          data/cassandra/BackendTests.cpp
          data/cassandra/BaseTests.cpp
          # Test runner
          TestGlobals.cpp
          Main.cpp
)

# Fix for dwarf5 bug on ci. IS STILL NEEDED???
target_compile_options(clio_options INTERFACE -gdwarf-4)

target_include_directories(clio_dbtests PRIVATE .)
target_link_libraries(clio_dbtests PUBLIC clio_testing_common)
set_target_properties(clio_dbtests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
