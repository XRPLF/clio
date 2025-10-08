# Clio API server

## Introduction

Clio is an XRP Ledger API server optimized for RPC calls over WebSocket or JSON-RPC.

It stores validated historical ledger and transaction data in a more space efficient format, and uses up to 4 times
less space than [rippled](https://github.com/XRPLF/rippled).

Clio can be configured to store data in [Apache Cassandra](https://cassandra.apache.org/_/index.html) or
[ScyllaDB](https://www.scylladb.com/), enabling scalable read throughput. Multiple Clio nodes can share
access to the same dataset, which allows for a highly available cluster of Clio nodes without the need for redundant
data storage or computation.

## Develop

As you prepare to develop code for Clio, please be sure you are aware of our current
[Contribution guidelines](https://github.com/XRPLF/clio/blob/develop/CONTRIBUTING.md).

Read about @ref "rpc" carefully to know more about writing your own handlers for Clio.
