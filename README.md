**Status:** This software is in beta mode. We encourage anyone to try it out and
report any issues they discover. Version 1.0 coming soon.

# clio
clio is an XRP Ledger API server. clio is optimized for RPC calls, over websocket or JSON-RPC. Validated
historical ledger and transaction data is stored in a more space efficient format,
using up to 4 times less space than rippled. clio can be configured to store data in Apache Cassandra or ScyllaDB,
allowing for scalable read throughput. Multiple clio nodes can share
access to the same dataset, allowing for a highly available cluster of clio nodes,
without the need for redundant data storage or computation.

clio offers the full rippled API, with the caveat that clio by default only returns validated data.
This means that `ledger_index` defaults to `validated` instead of `current` for all requests.
Other non-validated data is also not returned, such as information about queued transactions.
For requests that require access to the p2p network, such as `fee` or `submit`, clio automatically forwards the request to a rippled node, and propagates the response back to the client. To access non-validated data for *any* request, simply add `ledger_index: "current"` to the request, and clio will forward the request to rippled.

clio does not connect to the peer to peer network. Instead, clio extracts data from a specified rippled node. Running clio requires access to a rippled node
from which data can be extracted. The rippled node does not need to be running on the same machine as clio.


## Requirements
1. Access to a Cassandra cluster or ScyllaDB cluster. Can be local or remote.

2. Access to one or more rippled nodes. Can be local or remote.

## Building
clio is built with cmake. clio requires c++20, and boost 1.75.0 or later. protobuf v2 or v3 can both be used, however, users with older systems will need to update to GCC 8 or later and cmake 3.16.3 or later.
Use these instructions to build a clio executable from source. These instructions were tested on Ubuntu 20.04 LTS.

```
1. sudo apt-get update
2. sudo apt-get -y upgrade
3. sudo apt-get -y install git pkg-config protobuf-compiler libprotobuf-dev libssl-dev wget build-essential bison flex autoconf cmake
4. Boost:
  wget -O ~/boost_1_75_0.tar.gz https://boostorg.jfrog.io/artifactory/main/release/1.75.0/source/boost_1_75_0.tar.gz
  tar xvzf boost_1_75_0.tar.gz
  cd boost_1_75_0
  ./bootstrap.sh
  ./b2 -j$((`nproc`+1))
  # Add the following 'export' command
  # to your profile file (~/.profile):
  # -------------------------------
  export BOOST_ROOT=/home/my_user/boost_1_75_0
  source ~/.profile
5. cd ~
6. git clone https://github.com/XRPLF/clio.git
7. cd clio
8. mkdir build
9. cd build
10. cmake ..
11. cmake --build . -- -j$((`nproc`+1))
```

## Running
`./clio_server config.json`

clio needs access to a rippled server. The config files of rippled and clio need
to match in a certain sense.
clio needs to know:
- the ip of rippled
- the port on which rippled is accepting unencrypted websocket connections
- the port on which rippled is handling gRPC requests

rippled needs to open:
- a port to accept unencrypted websocket connections
- a port to handle gRPC requests, with the ip(s) of clio specified in the `secure_gateway` entry

The example configs of rippled and clio are setup such that minimal changes are
required. When running locally, the only change needed is to uncomment the `port_grpc`
section of the rippled config. When running clio and rippled on separate machines,
in addition to uncommenting the `port_grpc` section, a few other steps must be taken:
1. change the `ip` of the first entry of `etl_sources` to the ip where your rippled
server is running
2. open a public, unencrypted websocket port on your rippled server
3. change the ip specified in `secure_gateway` of `port_grpc` section of the rippled config
to the ip of your clio server. This entry can take the form of a comma separated list if
you are running multiple clio nodes.

Once your config files are ready, start rippled and clio. It doesn't matter which you
start first, and it's fine to stop one or the other and restart at any given time.

clio will wait for rippled to sync before extracting any ledgers. If there is already
data in clio's database, clio will begin extraction with the ledger whose sequence
is one greater than the greatest sequence currently in the database. clio will wait
for this ledger to be available. Be aware that the behavior of rippled is to sync to
the most recent ledger on the network, and then backfill. If clio is extracting ledgers
from rippled, and then rippled is stopped for a significant amount of time and then restarted, rippled
will take time to backfill to the next ledger that clio wants. The time it takes is proportional
to the amount of time rippled was offline for. Also be aware that the amount rippled backfills
is dependent on the online_delete and ledger_history config values; if these values
are small, and rippled is stopped for a significant amount of time, rippled may never backfill
to the ledger that clio wants. To avoid this situation, it is advised to keep history
proportional to the amount of time that you expect rippled to be offline. For example, if you
expect rippled to be offline for a few days from time to time, you should keep at least
a few days of history. If you expect rippled to never be offline, then you can keep a very small
amount of history.

clio can use multiple rippled servers as a data source. Simply add more entries to
the `etl_sources` section. clio will load balance requests across the servers specified
in this list. As long as one rippled server is up and synced, clio will continue
extracting ledgers.

In contrast to rippled, clio will answer RPC requests for the data already in the
database as soon as the server starts. clio doesn't wait to sync to the network, or
for rippled to sync.

When starting clio with a fresh database, clio needs to download a ledger in full.
This can take some time, and depends on database throughput. With a moderately fast
database, this should take less than 10 minutes. If you did not properly set `secure_gateway`
in the `port_grpc` section of rippled, this step will fail. Once the first ledger
is fully downloaded, clio only needs to extract the changed data for each ledger,
so extraction is much faster and clio can keep up with rippled in real time. Even under
intense load, clio should not lag behind the network, as clio is not processing the data,
and is simply writing to a database. The throughput of clio is dependent on the throughput
of your database, but a standard Cassandra or Scylla deployment can handle
the write load of the XRP Ledger without any trouble. Generally the performance considerations
come on the read side, and depends on the number of RPC requests your clio nodes
are serving. Be aware that very heavy read traffic can impact write throughput. Again, this
is on the database side, so if you are seeing this, upgrade your database.

It is possible to run multiple clio nodes that share access to the same database.
The clio nodes don't need to know about each other. You can simply spin up more clio
nodes pointing to the same database as you wish, and shut them down as you wish.
On startup, each clio node queries the database for the latest ledger. If this latest
ledger does not change for some time, the clio node begins extracting ledgers
and writing to the database. If the clio node detects a ledger that it is trying to
write has already been written, the clio node will backoff and stop writing. If later
the clio node sees no ledger written for some time, it will start writing again.
This algorithm ensures that at any given time, one and only one clio node is writing
to the database.

It is possible to force clio to only read data, and to never become a writer.
To do this, set `read_only: true` in the config. One common setup is to have a
small number of writer nodes that are inaccessible to clients, with several
read only nodes handling client requests. The number of read only nodes can be scaled
up or down in response to request volume.

When using multiple rippled servers as data sources and multiple clio nodes,
each clio node should use the same set of rippled servers as sources. The order doesn't matter.
The only reason not to do this is if you are running servers in different regions, and
you want the clio nodes to extract from servers in their region. However, if you
are doing this, be aware that database traffic will be flowing across regions,
which can cause high latencies. A possible alternative to this is to just deploy
a database in each region, and the clio nodes in each region use their region's database.
This is effectively two systems.
