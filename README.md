# Clio
Clio is an XRP Ledger API server. Clio is optimized for RPC calls, over WebSocket or JSON-RPC. Validated
historical ledger and transaction data are stored in a more space-efficient format,
using up to 4 times less space than rippled. Clio can be configured to store data in Apache Cassandra or ScyllaDB,
allowing for scalable read throughput. Multiple Clio nodes can share
access to the same dataset, allowing for a highly available cluster of Clio nodes,
without the need for redundant data storage or computation.

Clio offers the full rippled API, with the caveat that Clio by default only returns validated data.
This means that `ledger_index` defaults to `validated` instead of `current` for all requests.
Other non-validated data is also not returned, such as information about queued transactions.
For requests that require access to the p2p network, such as `fee` or `submit`, Clio automatically forwards the request to a rippled node and propagates the response back to the client. To access non-validated data for *any* request, simply add `ledger_index: "current"` to the request, and Clio will forward the request to rippled.

Clio does not connect to the peer-to-peer network. Instead, Clio extracts data from a group of specified rippled nodes. Running Clio requires access to at least one rippled node
from which data can be extracted. The rippled node does not need to be running on the same machine as Clio.


## Requirements
1. Access to a Cassandra cluster or ScyllaDB cluster. Can be local or remote.

2. Access to one or more rippled nodes. Can be local or remote.

## Building

Clio is built with CMake. Clio requires at least GCC-11 (C++20), and Boost 1.75.0 or later.

Use these instructions to build a Clio executable from the source. These instructions were tested on Ubuntu 20.04 LTS.

```sh
# Install dependencies
  sudo apt-get -y install git pkg-config protobuf-compiler libprotobuf-dev libssl-dev wget build-essential bison flex autoconf cmake

# Compile Boost
  wget -O $HOME/boost_1_75_0.tar.gz https://boostorg.jfrog.io/artifactory/main/release/1.75.0/source/boost_1_75_0.tar.gz
  tar xvzf $HOME/boost_1_75_0.tar.gz
  cd $HOME/boost_1_75_0
  ./bootstrap.sh
  ./b2 -j$(nproc)
  echo "export BOOST_ROOT=$HOME/boost_1_75_0" >> $HOME/.profile && source $HOME/.profile

# Clone the Clio Git repository & build Clio
  cd $HOME
  git clone https://github.com/XRPLF/clio.git
  cd $HOME/clio
  cmake -B build && cmake --build build --parallel $(nproc)
```

## Running
```sh
./clio_server config.json
```

Clio needs access to a rippled server. The config files of rippled and Clio need
to match in a certain sense.
Clio needs to know:
- the IP of rippled
- the port on which rippled is accepting unencrypted WebSocket connections
- the port on which rippled is handling gRPC requests

rippled needs to open:
- a port to accept unencrypted websocket connections
- a port to handle gRPC requests, with the IP(s) of Clio specified in the `secure_gateway` entry

The example configs of rippled and Clio are setups such that minimal changes are
required. When running locally, the only change needed is to uncomment the `port_grpc`
section of the rippled config. When running Clio and rippled on separate machines,
in addition to uncommenting the `port_grpc` section, a few other steps must be taken:
1. change the `ip` of the first entry of `etl_sources` to the IP where your rippled
server is running
2. open a public, unencrypted WebSocket port on your rippled server
3. change the IP specified in `secure_gateway` of `port_grpc` section of the rippled config
to the IP of your Clio server. This entry can take the form of a comma-separated list if
you are running multiple Clio nodes.

Once your config files are ready, start rippled and Clio. It doesn't matter which you
start first, and it's fine to stop one or the other and restart at any given time.

Clio will wait for rippled to sync before extracting any ledgers. If there is already
data in Clio's database, Clio will begin extraction with the ledger whose sequence
is one greater than the greatest sequence currently in the database. Clio will wait
for this ledger to be available. Be aware that the behavior of rippled is to sync to
the most recent ledger on the network, and then backfill. If Clio is extracting ledgers
from rippled, and then rippled is stopped for a significant amount of time and then restarted, rippled
will take time to backfill to the next ledger that Clio wants. The time it takes is proportional
to the amount of time rippled was offline for. Also be aware that the amount rippled backfills
are dependent on the online_delete and ledger_history config values; if these values
are small, and rippled is stopped for a significant amount of time, rippled may never backfill
to the ledger that Clio wants. To avoid this situation, it is advised to keep history
proportional to the amount of time that you expect rippled to be offline. For example, if you
expect rippled to be offline for a few days from time to time, you should keep at least
a few days of history. If you expect rippled to never be offline, then you can keep a very small
amount of history.

Clio can use multiple rippled servers as a data source. Simply add more entries to
the `etl_sources` section. Clio will load balance requests across the servers specified
in this list. As long as one rippled server is up and synced, Clio will continue
extracting ledgers.

In contrast to rippled, Clio will answer RPC requests for the data already in the
database as soon as the server starts. Clio doesn't wait to sync to the network, or
for rippled to sync.

When starting Clio with a fresh database, Clio needs to download a ledger in full.
This can take some time, and depends on database throughput. With a moderately fast
database, this should take less than 10 minutes. If you did not properly set `secure_gateway`
in the `port_grpc` section of rippled, this step will fail. Once the first ledger
is fully downloaded, Clio only needs to extract the changed data for each ledger,
so extraction is much faster and Clio can keep up with rippled in real-time. Even under
intense load, Clio should not lag behind the network, as Clio is not processing the data,
and is simply writing to a database. The throughput of Clio is dependent on the throughput
of your database, but a standard Cassandra or Scylla deployment can handle
the write load of the XRP Ledger without any trouble. Generally the performance considerations
come on the read side, and depends on the number of RPC requests your Clio nodes
are serving. Be aware that very heavy read traffic can impact write throughput. Again, this
is on the database side, so if you are seeing this, upgrade your database.

It is possible to run multiple Clio nodes that share access to the same database.
The Clio nodes don't need to know about each other. You can simply spin up more Clio
nodes pointing to the same database as you wish, and shut them down as you wish.
On startup, each Clio node queries the database for the latest ledger. If this latest
ledger does not change for some time, the Clio node begins extracting ledgers
and writing to the database. If the Clio node detects a ledger that it is trying to
write has already been written, the Clio node will backoff and stop writing. If later
the Clio node sees no ledger written for some time, it will start writing again.
This algorithm ensures that at any given time, one and only one Clio node is writing
to the database.

It is possible to force Clio to only read data, and to never become a writer.
To do this, set `read_only: true` in the config. One common setup is to have a
small number of writer nodes that are inaccessible to clients, with several
read only nodes handling client requests. The number of read only nodes can be scaled
up or down in response to request volume.

When using multiple rippled servers as data sources and multiple Clio nodes,
each Clio node should use the same set of rippled servers as sources. The order doesn't matter.
The only reason not to do this is if you are running servers in different regions, and
you want the Clio nodes to extract from servers in their region. However, if you
are doing this, be aware that database traffic will be flowing across regions,
which can cause high latencies. A possible alternative to this is to just deploy
a database in each region, and the Clio nodes in each region use their region's database.
This is effectively two systems.

## Developing against `rippled` in standalone mode

If you wish you develop against a `rippled` instance running in standalone
mode there are a few quirks of both clio and rippled you need to keep in mind.
You must:

1. Advance the `rippled` ledger to at least ledger 256
2. Wait 10 minutes before first starting clio against this standalone node.

## Logging
Clio provides several logging options, all are configurable via the config file and are detailed below.

`log_level`: The minimum level of severity at which the log message will be outputted.
Severity options are `trace`, `debug`, `info`, `warning`, `error`, `fatal`. Defaults to `info`.

`log_to_console`: Enable/disable log output to console. Options are `true`/`false`. Defaults to true.

`log_directory`: Path to the directory where log files are stored. If such directory doesn't exist, Clio will create it. If not specified, logs are not written to a file.

`log_rotation_size`: The max size of the log file in **megabytes** before it will rotate into a smaller file. Defaults to 2GB.

`log_directory_max_size`: The max size of the log directory in **megabytes** before old log files will be
deleted to free up space. Defaults to 50GB.

`log_rotation_hour_interval`: The time interval in **hours** after the last log rotation to automatically
rotate the current log file. Defaults to 12 hours.

Note, time-based log rotation occurs dependently on size-based log rotation, where if a
size-based log rotation occurs, the timer for the time-based rotation will reset.

## Cassandra / Scylla Administration

Since Clio relies on either Cassandra or Scylla for its database backend, here are some important considerations:

- Scylla, by default, will reserve all free RAM on a machine for itself. If you are running `rippled` or other services on the same machine, restrict its memory usage using the `--memory` argument: https://docs.scylladb.com/getting-started/scylla-in-a-shared-environment/
