# clio
clio is an XRP ledger history server. 

clio is designed to store historical ledger and transaction data in a more space efficient format, using up to 4 times less space than rippled.

clio offers the full rippled API (this is still in development at the time of writing), with the caveat that clio by default only returns validated data.
This means that `ledger_index` defaults to `validated` instead of `current` for all requests.
Other non-validated data is also not returned, such as information about queued transactions. 
For requests that require access to the p2p network, such as `fee` or `submit`, clio automatically forwards the request to a rippled node, and propagates the response back to the client. To access non-validated data for *any* request, simply add `ledger_index: "current"` to the request, and clio will forward the request to rippled.

clio does not connect to the peer to peer network. Instead, clio extracts data from a specified rippled node. Running clio requires access to a rippled node
from which data can be extracted. The rippled node does not need to be running on the same machine as clio.
clio can operate in read-only mode in the absence of a rippled node, where clio can answer RPC requests for the data already in the database.


clio is designed with scalability and availability as a first principle. 
Data is stored in either Postgres or Cassandra,
and multiple clio servers can share access to the same dataset.
The different clio servers that are using the same dataset do not know about each other or talk to each other.
At any given time, there is only one writer, and any synchronization happens via the data model at the database level.
If the writer for a given dataset fails for any reason, one of the other clio nodes will automatically become the writer.

## Requirements
1. Access to a Postgres server or Cassandra cluster. Can be local or remote.

2. Access to one or more rippled nodes. Can be local or remote.

## Building
clio is built with cmake. clio requires c++20, and boost 1.75.0 or later. clio includes rippled as a submodule.

```
git submodule update --init --recursive
mdkir build
cd build
cmake -DCMAKE_C_COMPILER=<your c compiler> -DCMAKE_CXX_COMPILER=<your c++ compiler that supports c++20> -DBOOST_ROOT=<location of boost> ..
cmake --build . -- -j 8
```

## Running
` ./clio_server config.json`
