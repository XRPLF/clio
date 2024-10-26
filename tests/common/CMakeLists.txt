add_library(clio_testing_common)

target_sources(
  clio_testing_common
  PRIVATE util/AssignRandomPort.cpp
          util/CallWithTimeout.cpp
          util/StringUtils.cpp
          util/TestHttpClient.cpp
          util/TestHttpServer.cpp
          util/TestObject.cpp
          util/TestWebSocketClient.cpp
          util/TestWsServer.cpp
)

include(deps/gtest)

target_include_directories(clio_testing_common PUBLIC .)
target_link_libraries(clio_testing_common PUBLIC clio_app gtest::gtest)
