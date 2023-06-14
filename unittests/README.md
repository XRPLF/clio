# Unit Testing
The correctness of new implementations can be verified via running unit tests. Below are the information on how to run unit tests.
## Requirements
### Cassandra cluster
If you wish to test the backend component you will need to have access to a **local (127.0.0.1)** Cassandra cluster, opened at port **9042**. Please ensure that the cluster is successfully running before running unit tests unless you filter out all `Backend` tests.

## Running
To run the unit tests, first build Clio as normal, then execute `./clio_tests` to run all unit tests.

**Note:** If you don't want to test the Cassandra backend code, the relevant tests can be disabled like this: `./clio_tests --gtest_filter="-BackendCassandraBaseTest*:BackendCassandraTest*:BackendCassandraFactoryTestWithDB*"`

# Adding Unit Tests
To add unit tests, please create a separate file for the component you are trying to cover (unless it already exists) and use any other existing unit test file as an example.
