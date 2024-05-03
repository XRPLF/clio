add_library(clio_testing_common)

target_sources(
  clio_testing_common
  PRIVATE util/StringUtils.cpp
          util/TestHttpServer.cpp
          util/TestWsServer.cpp
          util/TestObject.cpp
)

include(deps/gtest)

target_include_directories(clio_testing_common PUBLIC .)
target_link_libraries(clio_testing_common PUBLIC clio gtest::gtest)
