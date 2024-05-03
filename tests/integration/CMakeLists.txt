add_executable(clio_integration_tests)

target_sources(
  clio_integration_tests
  PRIVATE data/BackendFactoryTests.cpp data/cassandra/BackendTests.cpp data/cassandra/BaseTests.cpp
          # Test runner
          TestGlobals.cpp Main.cpp
)

# Fix for dwarf5 bug on ci. IS STILL NEEDED???
target_compile_options(clio_options INTERFACE -gdwarf-4)

target_include_directories(clio_integration_tests PRIVATE .)
target_link_libraries(clio_integration_tests PUBLIC clio_testing_common)
set_target_properties(clio_integration_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
