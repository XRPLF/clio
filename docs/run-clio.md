# How to run Clio

## Prerequisites

- Access to a Cassandra cluster or ScyllaDB cluster. Can be local or remote.
> [!IMPORTANT]
> There are some key considerations when using **ScyllaDB**. By default, Scylla reserves all free RAM on a machine for itself. If you are running `rippled` or other services on the same machine, restrict its memory usage using the `--memory` argument.
>
> See [ScyllaDB in a Shared Environment](https://docs.scylladb.com/getting-started/scylla-in-a-shared-environment/) to learn more.

- Access to one or more `rippled` nodes. Can be local or remote.

## Starting `rippled` and Clio

To run Clio you must first make the necessary changes to your configuration file, `config.json`. See [How to configure Clio and rippled](./configure-clio.md) to learn more.

Once your config files are ready, start `rippled` and Clio.

> [!TIP]
> It doesn't matter which you start first, and it's fine to stop one or the other and restart at any given time.

To start Clio, simply run:

```sh
./clio_server config.json
```

Clio will wait for `rippled` to sync before extracting any ledgers. If there is already data in Clio's database, Clio will begin extraction with the ledger whose sequence is one greater than the greatest sequence currently in the database. Clio will wait for this ledger to be available.

## Extracting ledgers from `rippled`

Be aware that the behavior of `rippled` is to sync to the most recent ledger on the network, and then backfill. If Clio is extracting ledgers from `rippled`, and then `rippled` is stopped for a significant amount of time and then restarted, `rippled` will take time to backfill to the next ledger that Clio wants.

The time it takes is proportional to the amount of time `rippled` was offline for. Additionally, the amount `rippled` backfills is dependent on the `online_delete` and `ledger_history` config values. If these values are small, and `rippled` is stopped for a significant amount of time, `rippled` may never backfill to the ledger that Clio wants.
To avoid this situation, it is advised to keep history proportional to the amount of time that you expect rippled to be offline. For example, if you expect `rippled` to be offline for a few days from time to time, you should keep at least a few days of history. If you expect `rippled` to never be offline, then you can keep a very small
amount of history.

Clio can use multiple `rippled` servers as a data source. Simply add more entries to the `etl_sources` section, and Clio will load balance requests across the servers specified in this list. As long as one `rippled` server is up and synced, Clio will continue extracting ledgers.

In contrast to `rippled`, Clio answers RPC requests for the data already in the database as soon as the server starts. Clio does not wait to sync to the network, or for `rippled` to sync.

## Starting Clio with a fresh database

When starting Clio with a fresh database, Clio needs to download a ledger in full.
This can take some time, and depends on database throughput. With a moderately fast database, this should take less than 10 minutes. If you did not properly set `secure_gateway` in the `port_grpc` section of `rippled`, this step will fail.

Once the first ledger is fully downloaded, Clio only needs to extract the changed data for each ledger, so extraction is much faster and Clio can keep up with `rippled` in real-time. Even under intense load, Clio should not lag behind the network, as Clio is not processing the data, and is simply writing to a database. The throughput of Clio is dependent on the throughput of your database, but a standard Cassandra or Scylla deployment can handle the write load of the XRP Ledger without any trouble.

> [!IMPORTANT]
> Generally the performance considerations come on the read side, and depend on the number of RPC requests your Clio nodes are serving. Be aware that very heavy read traffic can impact write throughput. Again, this is on the database side, so if you are seeing this, upgrade your database.

## Running multiple Clio nodes

It is possible to run multiple Clio nodes that share access to the same database. The Clio nodes don't need to know about each other. You can simply spin up more Clio nodes pointing to the same database, and shut them down as you wish.

On startup, each Clio node queries the database for the latest ledger. If this latest ledger does not change for some time, the Clio node begins extracting ledgers and writing to the database. If the Clio node detects a ledger that it is trying to write has already been written, the Clio node will backoff and stop writing. If the node does not see a ledger written for some time, it will start writing again. This algorithm ensures that at any given time, one and only one Clio node is writing to the database.

### Configuring read only Clio nodes

It is possible to force Clio to only read data, and to never become a writer. To do this, set `read_only: true` in the config. One common setup is to have a small number of writer nodes that are inaccessible to clients, with several read only nodes handling client requests. The number of read only nodes can be scaled up or down in response to request volume.

### Running multiple `rippled` servers

When using multiple `rippled` servers as data sources and multiple Clio nodes, each Clio node should use the same set of `rippled` servers as sources. The order doesn't matter. The only reason not to do this is if you are running servers in different regions, and you want the Clio nodes to extract from servers in their region. However, if you are doing this, be aware that database traffic will be flowing across regions, which can cause high latencies. A possible alternative to this is to just deploy a database in each region, and the Clio nodes in each region use their region's database. This is effectively two systems.

Clio supports API versioning as [described here](https://xrpl.org/request-formatting.html#api-versioning).
It's possible to configure `minimum`, `maximum` and `default` version like so:

```json
"api_version": {
    "min": 1,
    "max": 2,
    "default": 1
}
```

All of the above are optional.

Clio will fallback to hardcoded defaults when these values are not specified in the config file, or if the configured values are outside of the minimum and maximum supported versions hardcoded in [src/rpc/common/APIVersion.h](../src/rpc/common/APIVersion.hpp).

> [!TIP]
> See the [example-config.json](../example-config.json) for more details.
