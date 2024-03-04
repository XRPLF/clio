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
  PRIVATE # Boost::headers Boost::coroutine Boost::program_options Boost::system Boost::log Boost::log_setup
          # Boost::stacktrace_backtrace fmt::fmt OpenSSL::Crypto OpenSSL::SSL xrpl::libxrpl dl
          # libbacktrace::libbacktrace
  PUBLIC cassandra-cpp-driver::cassandra-cpp-driver clio_util
  # INTERFACE Threads::Threads
)
