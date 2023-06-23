target_link_libraries(clio_tests PUBLIC clio ${CONAN_LIBS_GTEST})
target_include_directories(clio_tests PRIVATE ${CONAN_INCLUDE_DIRS_GTEST})

# target_link_libraries(clio_tests PUBLIC clio gmock_main)

enable_testing()

include(GoogleTest)

#increase timeout for tests discovery to 10 seconds, by default it is 5s. As more unittests added, we start to hit this issue
#https://github.com/google/googletest/issues/3475
gtest_discover_tests(clio_tests DISCOVERY_TIMEOUT 10)
