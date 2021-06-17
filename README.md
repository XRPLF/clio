# clio
clio is an XRP ledger history server. 

clio is designed to store historical ledger and transaction data in a more space efficient format, using up to 4 times less space than rippled.

clio offers the full rippled API (this is still in development at the time of writing).

clio does not connect to the peer to peer network. clio extracts data from a specified rippled node. Running clio requires access to a rippled node
from which data can be extracted. The rippled node does not need to be running on the same machine as clio.
clio can operate in read-only mode in the absence of a rippled node, where clio can answer RPC requests for the data already in the database.

clio is designed with scalability and availability as a first principle. 
Data is stored in either Postgres or Cassandra,
and multiple clio servers can share access to the same dataset.
The different clio servers that are using the same dataset do not know about each other or talk to each other.
At any given time, there is only one writer, and any synchronization happens via the data model at the database level.
If the writer for a given dataset fails for any reason, one of the other clio nodes will become the writer.
This process is automatic.
The database itself can be scaled up or down as well, vertically or horizontally.

## Building
clio is built with cmake. clio requires c++20, and boost 1.75.0 (for boost json). clio includes rippled as a submodule.
Before building, run `git submodule update --init --recursive`
