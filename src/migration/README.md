
# Clio Migration 

Clio maintains the off-chain data of XRPL and multiple indexes tables to powering complex queries. To simplify the creation of index tables, this migration framework handles the process of database change and facilitates the migration of historical data seamlessly.


## Command Line Usage

Clio provides a migration command-line tool to migrate data in database. 


> Note: We need a **configuration file** to run the migration tool. This configuration file has the same format as the configuration file of the Clio server, ensuring consistency and ease of use. It reads the database configuration from the same session as the server's configuration, eliminating the need for separate setup or additional configuration files. Be aware that migration-specific configuration is under `.migration` session.


### To query migration status:
    
    
    ./clio_server --migrate status  ~/config/migrator.json
    
This command returns the current migration status of each migrator. The example output: 

    
    Current Migration Status:
    Migrator: ExampleMigrator - Feature v1, Clio v3 - not migrated
    

### To start a migration:
    
    
    ./clio_server --migrate ExampleMigrator  ~/config/migrator.json
    
    
Migration will run if the migrator has not been migrated. The migrator will be marked as migrated after the migration is completed.

## How to write a migrator

> **Note** If you'd like to add new index table in Clio and old historical data needs to be migrated into new table, you'd need to write a migrator.

A migrator satisfies the `MigratorSpec`(impl/Spec.hpp) concept.

It contains:

-  A `name` which will be used to identify the migrator. User will refer this migrator in command-line tool by this name. The name needs to be different with other migrators, otherwise a compilation error will be raised.

-  A `description` which is the detail information of the migrator.

- A static function `runMigration`, it will be called when user run `--migrate name`. It accepts two parameters: backend, which provides the DB operations interface, and cfg, which provides migration-related configuration. Each migrator can have its own configuration under `.migration` session.

- A type name alias `Backend` which specifies the backend type it supports.

> **Note** Each migrator is designed to work with a specific database.

- Register your migrator in MigrationManager. Currently we only support Cassandra/ScyllaDB.  Migrator needs to be registered in `CassandraSupportedMigrators`.


## How to use full table scanner (Only for Cassandra/ScyllaDB)
Sometimes migrator isn't able to query the historical data by table's partition key. For example, migrator of transactions needs the historical transaction data without knowing each transaction hash. Full table scanner can help to get all the rows in parallel. 

Most indexes are based on either ledger states or transactions. We provide the `objects` and `transactions` scanner. Developers only need to implement the callback function to receive the historical data. Please find the examples in `tests/integration/migration/cassandra/ExampleTransactionsMigrator.cpp` and `tests/integration/migration/cassandra/ExampleObjectsMigrator.cpp`.

> **Note** The full table scanner splits the table into multiple ranges by token(https://opensource.docs.scylladb.com/stable/cql/functions.html#token). A few of rows maybe read 2 times if its token happens to be at the edge of ranges. **Deduplication is needed** in the callback function.

## How to write a full table scan adapter (Only for Cassandra/ScyllaDB)

If you need to do full scan against other table, you can follow below steps:
- Describe the table which needs full scan in a struct. It has to satisfy the `TableSpec`(cassandra/Spec.hpp) concept, containing static member:
    - Tuple type `Row`, it's the type of each field in a row. The order of types should match what database will return in a row. Key types should come first, followed by other field types sorted in alphabetical order.
    - `PARTITION_KEY`, it's the name of the partition key of the table.
    - `TABLE_NAME`

- Inherent from `FullTableScannerAdapterBase`.
- Implement `onRowRead`, its parameter is the `Row` we defined. It's the callback function when a row is read.


Please take ObjectsAdapter/TransactionsAdapter as example.

## Examples:

We have some example migrators under `tests/integration/migration/cassandra` folder.

- ExampleDropTableMigrator

    This migrator drops `diff` table. 
- ExampleLedgerMigrator

    This migrator shows how to migrate data when we don't need to do full table scan. This migrator creates an index table `ledger_example` which maintains the map of ledger sequence and its account hash. 
- ExampleObjectsMigrator

    This migrator shows how to migrate ledger states related data. It uses `ObjectsScanner` to proceed the full scan in parallel. It counts the number of ACCOUNT_ROOT.
- ExampleTransactionsMigrator

    This migrator shows how to migrate transactions related data. It uses `TransactionsScanner` to proceed the `transactions` table full scan in parallel. It creates an index table `tx_index_example` which tracks the transaction hash and its according transaction type.

