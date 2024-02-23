# How to configure Clio and `rippled`

## Ports

Clio needs access to a `rippled` server in order to work. The following configurations are required for Clio and `rippled` to communicate:

1. In the Clio config file, provide the following:

   - The IP of the `rippled` server

   - The port on which `rippled` is accepting unencrypted WebSocket connections

   - The port on which `rippled` is handling gRPC requests

2. In the `rippled` config file, you need to open:

   - A port to accept unencrypted WebSocket connections

   - A port to handle gRPC requests, with the IP(s) of Clio specified in the `secure_gateway` entry

The example configs of [rippled](https://github.com/XRPLF/rippled/blob/develop/cfg/rippled-example.cfg) and [Clio](../docs/examples/config/example-config.json) are set up in a way that minimal changes are required.
When running locally, the only change needed is to uncomment the `port_grpc` section of the `rippled` config.

If you're running Clio and `rippled` on separate machines, in addition to uncommenting the `port_grpc` section, a few other steps must be taken:

1. Change the `ip` in `etl_sources` to the IP where your `rippled` server is running.

2. Open a public, unencrypted WebSocket port on your `rippled` server.

3. In the `rippled` config, change the IP specified for `secure_gateway`, under the `port_grpc` section, to the IP of your Clio server. This entry can take the form of a comma-separated list if you are running multiple Clio nodes.

## Ledger sequence

The parameter `start_sequence` can be included and configured within the top level of the config file. This parameter specifies the sequence of the first ledger to extract if the database is empty.

Note that ETL extracts ledgers in order, and backfilling functionality currently doesn't exist. This means Clio does not retroactively learn ledgers older than the one you specify. Choosing to specify this or not will yield the following behavior:

- If this setting is absent and the database is empty, ETL will start with the next ledger validated by the network.

- If this setting is present and the database is not empty, an exception is thrown.

In addition, the optional parameter `finish_sequence` can be added to the json file as well, specifying where the ledger can stop.

To add `start_sequence` and/or `finish_sequence` to the `config.json` file appropriately, they must be on the same top level of precedence as other parameters (i.e., `database`, `etl_sources`, `read_only`) and be specified with an integer.

Here is an example snippet from the config file:

```json
"start_sequence": 12345,
"finish_sequence": 54321
```

## SSL

The parameters `ssl_cert_file` and `ssl_key_file` can also be added to the top level of precedence of our Clio config. The `ssl_cert_file` field specifies the filepath for your SSL cert, while `ssl_key_file` specifies the filepath for your SSL key. It is up to you how to change ownership of these folders for your designated Clio user. 

Your options include:

- Copying the two files as root somewhere that's accessible by the Clio user, then running `sudo chown` to your user
- Changing the permissions directly so it's readable by your Clio user
- Running Clio as root (strongly discouraged)

Here is an example of how to specify `ssl_cert_file` and `ssl_key_file` in the config:

```json
"server": {
    "ip": "0.0.0.0",
    "port": 51233
},
"ssl_cert_file": "/full/path/to/cert.file",
"ssl_key_file": "/full/path/to/key.file"
```

## Admin rights for requests

By default Clio checks admin privileges by IP address from requests (only `127.0.0.1` is considered to be an admin). This is not very secure because the IP could be spoofed. For better security, an `admin_password` can be provided in the `server` section of Clio's config:

```json
"server": {
    "admin_password": "secret"
}
```

If the password is presented in the config, Clio will check the Authorization header (if any) in each request for the password. The Authorization header should contain the type `Password`, and the password from the config (e.g. `Password secret`).
Exactly equal password gains admin rights for the request or a websocket connection.

## ETL sources forwarding cache

By default Clio does not caches forwarding requests to ETL sources.
To enable the caching for a source, `forwarding_cache_timeout_ms` value should be added to the source entry in `etl_sources`:

```json
"etl_sources": [
    {
        "ip": "127.0.0.1",
        "ws_port": "6006",
        "grpc_port": "50051",
        "forwarding_cache_timeout_ms": 250
    }
],
```

`forwarding_cache_timeout_ms` defines for how long a cache entry will be valid after being placed into cache.
Zero value turns of the cache feature.
