# clio
clio is an XRP ledger history server. 

clio is designed to store historical ledger and transaction data in a more space efficient format, using up to 4 times less space than rippled.

clio offers the full rippled API (this is still in development at the time of writing).

clio does not connect to the peer to peer network. clio extracts data from a specified rippled node. Running clio requires access to a rippled node
from which data can be extracted. clio can operate in read-only mode in the absence of a rippled node, where clio can answer RPC requests for the data it already has.

clio is designed with scalability and availability as a first principle. Data is stored in either Postgres or Cassandra,
and multiple clio servers can share access to the same dataset. The different clio servers do not know about each other or talk to each other.
Any synchronization happens via the data model at the database level.
clio servers can be added or removed in a matter of seconds in order to scale up or down.
The database itself can be scaled up or down as well.

## Building
clio is built with cmake. clio requires c++20, and boost 1.75.0 (for boost json).
