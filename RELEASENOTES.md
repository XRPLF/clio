# Release Notes

This document contains the release notes for `clio_server`, an XRP Ledger API Server.

To build and run `clio_server`, follow the instructions in [README.md](https://github.com/cjcobb23/clio).

If you find issues or have a new idea, please open [an issue](https://github.com/cjcobb23/clio/issues).

# Releases

## 0.1.0

Clio is an XRP Ledger API server. Clio is optimized for RPC calls, over websocket or JSON-RPC. Validated historical ledger and transaction data is stored in a more space efficient format, using up to 4 times less space than rippled.

Clio uses Cassandra or ScyllaDB, allowing for scalable read throughput. Multiple clio nodes can share access to the same dataset, allowing for a highly available cluster of clio nodes, without the need for redundant data storage or computation.

**0.1.0** is the first beta of Project Clio. It contains:
-  `./src/backend` is the BackendInterface. This provides an abstraction for reading and writing information to a database.
-  `./src/etl` is the ReportingETL. The classes in this folder are used to extract information from the P2P network and write it to a database, either locally or over the network.
-  `./src/rpc` contains RPC handlers that are called by clients. These handlers should expose the same API as rippled.
-  `./src/subscriptions` contains the SubscriptionManager. This manages publishing to clients subscribing to streams or accounts.
-  `./src/webserver` contains a flex server that handles both http/s and ws/s traffic on a single port.
-  `./unittests` simple unit tests that write to and read from a database to verify that the ETL works.