add_library(clio_migration)

target_sources(
  clio_migration PRIVATE MigrationApplication.cpp MigrationManagerFactory.cpp cassandra/ObjectsAdapter.cpp
                         cassandra/TransactionsAdapter.cpp
)

target_link_libraries(clio_migration PRIVATE clio_util clio_etl)
