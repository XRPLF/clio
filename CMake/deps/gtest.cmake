target_link_libraries(clio_tests PUBLIC clio)
target_link_libraries(clio_tests PRIVATE CONAN_PKG::gtest) 

enable_testing()
include(GoogleTest)

# see https://github.com/google/googletest/issues/3475
gtest_discover_tests(clio_tests DISCOVERY_TIMEOUT 10)
