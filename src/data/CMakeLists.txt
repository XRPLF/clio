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

target_link_libraries(clio_data PUBLIC cassandra-cpp-driver::cassandra-cpp-driver clio_util)
