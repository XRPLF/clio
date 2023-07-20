find_package(gtest REQUIRED)
target_link_libraries(clio_tests PUBLIC clio gtest::gtest)

enable_testing()
include(GoogleTest)

# see https://github.com/google/googletest/issues/3475
gtest_discover_tests(clio_tests DISCOVERY_TIMEOUT 10)
