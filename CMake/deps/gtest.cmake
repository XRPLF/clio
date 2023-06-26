target_link_libraries(clio_tests PUBLIC clio ${CONAN_LIBS_GTEST})
target_include_directories(clio_tests PRIVATE ${CONAN_INCLUDE_DIRS_GTEST})

enable_testing()
include(GoogleTest)

# see https://github.com/google/googletest/issues/3475
gtest_discover_tests(clio_tests DISCOVERY_TIMEOUT 10)
