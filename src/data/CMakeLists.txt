add_library(clio_data)
target_sources(
  clio_data
  PRIVATE AmendmentCenter.cpp
          BackendCounters.cpp
          BackendInterface.cpp
          LedgerCache.cpp
          LedgerHeaderCache.cpp
          cassandra/impl/Future.cpp
          cassandra/impl/Cluster.cpp
          cassandra/impl/Batch.cpp
          cassandra/impl/Result.cpp
          cassandra/impl/Tuple.cpp
          cassandra/impl/SslContext.cpp
          cassandra/Handle.cpp
          cassandra/SettingsProvider.cpp
          clickhouse/SettingsProvider.cpp
          clickhouse/Handle.cpp
          clickhouse/impl/Result.cpp
          clickhouse/impl/Session.cpp
          clickhouse/impl/Cluster.cpp
          clickhouse/impl/Statement.cpp
          clickhouse/impl/Batch.cpp
          clickhouse/impl/PreparedStatement.cpp
          ClickHouseBackend.cpp
)

target_link_libraries(clio_data PUBLIC cassandra-cpp-driver::cassandra-cpp-driver clio_util)
