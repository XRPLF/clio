add_library(clio_etl OBJECT)

target_sources(
  clio_etl
  PRIVATE NFTHelpers.cpp
          ETLService.cpp
          ETLState.cpp
          LoadBalancer.cpp
          CacheLoaderSettings.cpp
          Source.cpp
          impl/ForwardingSource.cpp
          impl/GrpcSource.cpp
          impl/SubscriptionSource.cpp
)

target_include_directories(clio_etl PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
  clio_etl
  PUBLIC Boost::boost
         Boost::coroutine
         Boost::program_options
         Boost::system
         Boost::log
         Boost::log_setup
         Boost::stacktrace_backtrace
         cassandra-cpp-driver::cassandra-cpp-driver
         fmt::fmt
         OpenSSL::Crypto
         OpenSSL::SSL
         xrpl::libxrpl
         dl
         libbacktrace::libbacktrace
         clio_util
  INTERFACE Threads::Threads
)
