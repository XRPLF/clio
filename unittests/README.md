# Unit Testing
The correctness of new implementations can be verified via running unit tests. Below are the information on how to run unit tests.
## Requirements
### 1. Cassandra cluster
Have access to a **local (127.0.0.1)** Cassandra cluster, opened at port **9042**. Please ensure that the cluster is successfully running before running Unit Tests.
## Running
To run the unit tests, first build Clio as normal, then execute `./clio_tests` to run the unit tests.

## Tests
Below is a list of currently available unit tests. Please keep in mind that this list should be constantly updated with new unit tests as new features are added to the project.

- BackendTest.basic
- Backend.cache
- Backend.cacheBackground
- Backend.cacheIntegration

# Adding Unit Tests
To add unit tests, append a new test block in the unittests/main.cpp file with the following format:

```cpp
TEST(module_name, test_name)
{
    // Test code goes here
}
```
