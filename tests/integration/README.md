# Integration testing

Tests that hit the real database are separate from the unit test suite found under `tests/unit`.

## Requirements
### Cassandra/ScyllaDB cluster
If you wish to test the backend component you will need to have access to a **local (127.0.0.1)** Cassandra cluster, opened at port **9042**. Please ensure that the cluster is successfully running before running these tests.

## Running
To run the DB tests, first build Clio as normal, then execute `./clio_dbtests` to run all database tests.
