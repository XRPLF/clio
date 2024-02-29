add_library(clio_data)
target_sources(
  clio_data
  PRIVATE BackendCounters.cpp
          BackendInterface.cpp
          LedgerCache.cpp
          cassandra/impl/Future.cpp
          cassandra/impl/Cluster.cpp
          cassandra/impl/Batch.cpp
          cassandra/impl/Result.cpp
          cassandra/impl/Tuple.cpp
          cassandra/impl/SslContext.cpp
          cassandra/Handle.cpp
          cassandra/SettingsProvider.cpp
)

target_include_directories(clio_data PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(
  clio_data
  PUBLIC Boost::boost
  PUBLIC Boost::coroutine
  PUBLIC Boost::program_options
  PUBLIC Boost::system
  PUBLIC Boost::log
  PUBLIC Boost::log_setup
  PUBLIC Boost::stacktrace_backtrace
  PUBLIC cassandra-cpp-driver::cassandra-cpp-driver
  PUBLIC fmt::fmt
  PUBLIC OpenSSL::Crypto
  PUBLIC OpenSSL::SSL
  PUBLIC xrpl::libxrpl
  PUBLIC dl
  PUBLIC libbacktrace::libbacktrace
  INTERFACE Threads::Threads
)
