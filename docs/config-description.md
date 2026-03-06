# Clio Config Description

This document provides a list of all available Clio configuration properties in detail.

> [!NOTE]
> Dot notation in configuration key names represents nested fields.
> For example, **database.scylladb** refers to the _scylladb_ field inside the _database_ object.
> If a key name includes "[]", it indicates that the nested field is an array (e.g., etl_sources.[]).

## Configuration Details

### database.type

- **Required**: True
- **Type**: string
- **Default value**: `cassandra`
- **Constraints**: The value must be one of the following: `cassandra`.
- **Description**: Specifies the type of database used for storing and retrieving data required by the Clio server. Both ScyllaDB and Cassandra can serve as backends for Clio; however, this value must be set to `cassandra`.

### database.cassandra.contact_points

- **Required**: True
- **Type**: string
- **Default value**: `localhost`
- **Constraints**: None
- **Description**: A list of IP addresses or hostnames for the initial cluster nodes (Cassandra or ScyllaDB) that the client connects to when establishing a database connection. If you're running Clio locally, set this value to `localhost` or `127.0.0.1`.

### database.cassandra.secure_connect_bundle

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: The configuration file that contains the necessary credentials and connection details for securely connecting to a Cassandra database cluster.

### database.cassandra.port

- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `65535`.
- **Description**: The port number used to connect to the Cassandra database.

### database.cassandra.keyspace

- **Required**: True
- **Type**: string
- **Default value**: `clio`
- **Constraints**: None
- **Description**: The Cassandra keyspace to use for the database. If you don't provide a value, this is set to `clio` by default.

### database.cassandra.replication_factor

- **Required**: True
- **Type**: int
- **Default value**: `3`
- **Constraints**: The minimum value is `0`. The maximum value is `65535`.
- **Description**: Represents the number of replicated nodes for ScyllaDB. For more details see [Fault Tolerance Replication Factor](https://university.scylladb.com/courses/scylla-essentials-overview/lessons/high-availability/topic/fault-tolerance-replication-factor/).

### database.cassandra.table_prefix

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: An optional field to specify a prefix for the Cassandra database table names.

### database.cassandra.max_write_requests_outstanding

- **Required**: True
- **Type**: int
- **Default value**: `10000`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: Represents the maximum number of outstanding write requests. Write requests are API calls that write to the database.

### database.cassandra.max_read_requests_outstanding

- **Required**: True
- **Type**: int
- **Default value**: `100000`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: Maximum number of outstanding read requests. Read requests are API calls that read from the database.

### database.cassandra.threads

- **Required**: True
- **Type**: int
- **Default value**: The number of available CPU cores.
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: Represents the number of threads that will be used for database operations.

### database.cassandra.provider

- **Required**: True
- **Type**: string
- **Default value**: `cassandra`
- **Constraints**: The value must be one of the following: `cassandra`, `aws_keyspace`.
- **Description**: The specific database backend provider we are using.

### database.cassandra.core_connections_per_host

- **Required**: True
- **Type**: int
- **Default value**: `1`
- **Constraints**: The minimum value is `1`. The maximum value is `65535`.
- **Description**: The number of core connections per host for the Cassandra database.

### database.cassandra.queue_size_io

- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `65535`.
- **Description**: Defines the queue size of the input/output (I/O) operations in Cassandra.

### database.cassandra.write_batch_size

- **Required**: True
- **Type**: int
- **Default value**: `20`
- **Constraints**: The minimum value is `1`. The maximum value is `65535`.
- **Description**: Represents the batch size for write operations in Cassandra.

### database.cassandra.connect_timeout

- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The maximum amount of time in seconds that the system waits for a database connection to be established.

### database.cassandra.request_timeout

- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The maximum amount of time in seconds that the system waits for a request to be fetched from the database.

### database.cassandra.username

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: The username used for authenticating with the database.

### database.cassandra.password

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: The password used for authenticating with the database.

### database.cassandra.certfile

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: The path to the SSL/TLS certificate file used to establish a secure connection between the client and the Cassandra database.

### allow_no_etl

- **Required**: True
- **Type**: boolean
- **Default value**: `False`
- **Constraints**: None
- **Description**: If set to `True`, allows Clio to start without any ETL source.

### etl_sources.[].ip

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: The value must be a valid IP address.
- **Description**: The IP address of the ETL source.

### etl_sources.[].ws_port

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `65535`.
- **Description**: The WebSocket port of the ETL source.

### etl_sources.[].grpc_port

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `65535`.
- **Description**: The gRPC port of the ETL source.

### forwarding.cache_timeout

- **Required**: True
- **Type**: double
- **Default value**: `0`
- **Constraints**: The value must be a positive double number.
- **Description**: Specifies the timeout duration (in seconds) for the forwarding cache used in `rippled` communication. A value of `0` means disabling this feature.

### forwarding.request_timeout

- **Required**: True
- **Type**: double
- **Default value**: `10`
- **Constraints**: The value must be a positive double number.
- **Description**: Specifies the timeout duration (in seconds) for the forwarding request used in `rippled` communication.

### rpc.cache_timeout

- **Required**: True
- **Type**: double
- **Default value**: `0`
- **Constraints**: The value must be a positive double number.
- **Description**: Specifies the timeout duration (in seconds) for RPC cache response to timeout. A value of `0` means disabling this feature.

### num_markers

- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `256`.
- **Description**: Specifies the number of coroutines used to download the initial ledger.

### dos_guard.whitelist.[]

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: The list of IP addresses to whitelist for DOS protection.

### dos_guard.max_fetches

- **Required**: True
- **Type**: int
- **Default value**: `1000000`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The maximum number of fetch operations allowed by DOS guard.

### dos_guard.max_connections

- **Required**: True
- **Type**: int
- **Default value**: `20`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The maximum number of concurrent connections for a specific IP address.

### dos_guard.max_requests

- **Required**: True
- **Type**: int
- **Default value**: `20`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The maximum number of requests allowed for a specific IP address.

### dos_guard.sweep_interval

- **Required**: True
- **Type**: double
- **Default value**: `1`
- **Constraints**: The value must be a positive double number.
- **Description**: Interval in seconds for DOS guard to sweep(clear) its state.

### workers

- **Required**: True
- **Type**: int
- **Default value**: The number of available CPU cores.
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The number of threads used to process RPC requests.

### server.ip

- **Required**: True
- **Type**: string
- **Default value**: None
- **Constraints**: The value must be a valid IP address.
- **Description**: The IP address of the Clio HTTP server.

### server.port

- **Required**: True
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `65535`.
- **Description**: The port number of the Clio HTTP server.

### server.max_queue_size

- **Required**: True
- **Type**: int
- **Default value**: `1000`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The maximum size of the server's request queue. If set to `0`, this means there is no queue size limit.

### server.local_admin

- **Required**: False
- **Type**: boolean
- **Default value**: None
- **Constraints**: None
- **Description**: Indicates if requests from `localhost` are allowed to call Clio admin-only APIs. Note that this setting cannot be enabled together with [server.admin_password](#serveradmin_password).

### server.admin_password

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: The password for Clio admin-only APIs. Note that this setting cannot be enabled together with [server.local_admin](#serveradmin_password).

### server.processing_policy

- **Required**: True
- **Type**: string
- **Default value**: `parallel`
- **Constraints**: The value must be one of the following: `parallel`, `sequent`.
- **Description**: For the `sequent` policy, requests from a single client connection are processed one by one, with the next request read only after the previous one is processed. For the `parallel` policy, Clio will accept all requests and process them in parallel, sending a reply for each request as soon as it is ready.

### server.parallel_requests_limit

- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `65535`.
- **Description**: This is an optional parameter, used only if the `processing_strategy` is `parallel`. It limits the number of requests processed in parallel for a single client connection. If not specified, no limit is enforced.

### server.ws_max_sending_queue_size

- **Required**: True
- **Type**: int
- **Default value**: `1500`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: Maximum queue size for sending subscription data to clients. This queue buffers data when a client is slow to receive it, ensuring delivery once the client is ready.

### server.proxy.ips.[]

- **Required**: True
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: List of proxy ip addresses. When Clio receives a request from proxy it will use `Forwarded` value (if any) as client ip. When this option is used together with `server.proxy.tokens` Clio will identify proxy by ip or by token.

### server.proxy.tokens.[]

- **Required**: True
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: List of tokens in identifying request as a request from proxy. Token should be provided in `X-Proxy-Token` header, e.g. `X-Proxy-Token: <very_secret_token>'. When Clio receives a request from proxy it will use 'Forwarded` value (if any) to get client ip. When this option is used together with 'server.proxy.ips' Clio will identify proxy by ip or by token.

### prometheus.enabled

- **Required**: True
- **Type**: boolean
- **Default value**: `True`
- **Constraints**: None
- **Description**: Enables or disables Prometheus metrics.

### prometheus.compress_reply

- **Required**: True
- **Type**: boolean
- **Default value**: `True`
- **Constraints**: None
- **Description**: Enables or disables compression of Prometheus responses.

### io_threads

- **Required**: True
- **Type**: int
- **Default value**: `2`
- **Constraints**: The minimum value is `1`. The maximum value is `65535`.
- **Description**: The number of input/output (I/O) threads. The value cannot be less than `1`.

### subscription_workers

- **Required**: True
- **Type**: int
- **Default value**: `1`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The number of worker threads or processes that are responsible for managing and processing subscription-based tasks from `rippled`.

### graceful_period

- **Required**: True
- **Type**: double
- **Default value**: `10`
- **Constraints**: The value must be a positive double number.
- **Description**: The number of seconds the server waits to shutdown gracefully. If Clio does not shutdown gracefully after the specified value, it will be killed instead.

### cache.num_diffs

- **Required**: True
- **Type**: int
- **Default value**: `32`
- **Constraints**: The minimum value is `1`. The maximum value is `65535`.
- **Description**: The number of cursors generated is the number of changed (without counting deleted) objects in the latest `cache.num_diffs` number of ledgers. Cursors are workers that load the ledger cache from the position of markers concurrently. For more information, please read [README.md](../src/etl/README.md).

### cache.num_markers

- **Required**: True
- **Type**: int
- **Default value**: `48`
- **Constraints**: The minimum value is `1`. The maximum value is `65535`.
- **Description**: Specifies how many markers are placed randomly within the cache. These markers define the positions on the ledger that will be loaded concurrently by the workers. The higher the number, the more places within the cache we potentially cover.

### cache.num_cursors_from_diff

- **Required**: True
- **Type**: int
- **Default value**: `0`
- **Constraints**: The minimum value is `0`. The maximum value is `65535`.
- **Description**: `cache.num_cursors_from_diff` number of cursors are generated by looking at the number of changed objects in the most recent ledger. If number of changed objects in current ledger is not enough, it will keep reading previous ledgers until it hit `cache.num_cursors_from_diff`. If set to `0`, the system defaults to generating cursors based on `cache.num_diffs`.

### cache.num_cursors_from_account

- **Required**: True
- **Type**: int
- **Default value**: `0`
- **Constraints**: The minimum value is `0`. The maximum value is `65535`.
- **Description**: `cache.num_cursors_from_diff` of cursors are generated by reading accounts in `account_tx` table. If set to `0`, the system defaults to generating cursors based on `cache.num_diffs`.

### cache.page_fetch_size

- **Required**: True
- **Type**: int
- **Default value**: `512`
- **Constraints**: The minimum value is `1`. The maximum value is `65535`.
- **Description**: The number of ledger objects to fetch concurrently per marker.

### cache.limit_load_in_cluster

- **Required**: True
- **Type**: boolean
- **Default value**: `False`
- **Constraints**: None
- **Description**: If enabled only one clio node in a cluster (sharing the same database) will load cache at a time

### cache.load

- **Required**: True
- **Type**: string
- **Default value**: `async`
- **Constraints**: The value must be one of the following: `sync`, `async`, `none`.
- **Description**: The strategy used for Cache loading.

### cache.file.path

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: The path to a file where cache will be saved to on shutdown and loaded from on startup. If the file couldn't be read Clio will load cache as usual (from DB or from rippled).

### cache.file.max_sequence_age

- **Required**: True
- **Type**: int
- **Default value**: `5000`
- **Constraints**: None
- **Description**: Max allowed difference between the latest sequence in DB and in cache file. If the cache file is too old (contains too low latest sequence) Clio will reject using it.

### cache.file.async_save

- **Required**: True
- **Type**: boolean
- **Default value**: `False`
- **Constraints**: None
- **Description**: When false, Clio waits for cache saving to finish before shutting down. When true, cache saving runs in parallel with other shutdown operations.

### log.channels.[].channel

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: The value must be one of the following: `General`, `WebServer`, `Backend`, `RPC`, `ETL`, `Subscriptions`, `Performance`, `Migration`.
- **Description**: The name of the log channel.

### log.channels.[].level

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: The value must be one of the following: `trace`, `debug`, `info`, `warning`, `error`, `fatal`.
- **Description**: The log level for the specific log channel.

### log.level

- **Required**: True
- **Type**: string
- **Default value**: `info`
- **Constraints**: The value must be one of the following: `trace`, `debug`, `info`, `warning`, `error`, `fatal`.
- **Description**: The general logging level of Clio. This level is applied to all log channels that do not have an explicitly defined logging level.

### log.format

- **Required**: True
- **Type**: string
- **Default value**: `%Y-%m-%d %H:%M:%S.%f %^%3!l:%n%$ - %v`
- **Constraints**: None
- **Description**: The format string for log messages using spdlog format patterns.

Each of the variables expands like so:

- `%Y-%m-%d %H:%M:%S.%f`: The full date and time of the log entry with microsecond precision
- `%^`: Start color range
- `%3!l`: The severity (aka log level) the entry was sent at stripped to 3 characters
- `%n`: The logger name (channel) that this log entry was sent to
- `%$`: End color range
- `%v`: The actual log message

Some additional variables that might be useful:

- `%@`: A partial path to the C++ file and the line number in the said file (`src/file/path:linenumber`)
- `%t`: The ID of the thread the log entry is written from

Documentation can be found at: <https://github.com/gabime/spdlog/wiki/Custom-formatting>.

### log.is_async

- **Required**: True
- **Type**: boolean
- **Default value**: `True`
- **Constraints**: None
- **Description**: Whether spdlog is asynchronous or not.

### log.enable_console

- **Required**: True
- **Type**: boolean
- **Default value**: `False`
- **Constraints**: None
- **Description**: Enables or disables logging to the console.

### log.directory

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: The directory path for the log files.

### log.rotation_size

- **Required**: True
- **Type**: int
- **Default value**: `2048`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The log rotation size in megabytes. When the log file reaches this particular size, a new log file starts.

### log.directory_max_files

- **Required**: True
- **Type**: int
- **Default value**: `25`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The maximum number of log files in the directory.

### log.tag_style

- **Required**: True
- **Type**: string
- **Default value**: `none`
- **Constraints**: The value must be one of the following: `int`, `uint`, `null`, `none`, `uuid`.
- **Description**: Log tags are unique identifiers for log messages. `uint`/`int` starts logging from 0 and increments, making it faster. In contrast, `uuid` generates a random unique identifier, which adds overhead.

### extractor_threads

- **Required**: True
- **Type**: int
- **Default value**: `1`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: Number of threads used to extract data from ETL source.

### read_only

- **Required**: True
- **Type**: boolean
- **Default value**: `False`
- **Constraints**: None
- **Description**: Indicates if the server is allowed to write data to the database.

### start_sequence

- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: If specified, the ledger index Clio will start writing to the database from.

### finish_sequence

- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: If specified, the final ledger that Clio will write to the database.

### ssl_cert_file

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: The path to the SSL certificate file.

### ssl_key_file

- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
- **Description**: The path to the SSL key file.

### api_version.default

- **Required**: True
- **Type**: int
- **Default value**: `1`
- **Constraints**: The minimum value is `1`. The maximum value is `3`.
- **Description**: The default API version that the Clio server will run on.

### api_version.min

- **Required**: True
- **Type**: int
- **Default value**: `1`
- **Constraints**: The minimum value is `1`. The maximum value is `3`.
- **Description**: The minimum API version allowed to use.

### api_version.max

- **Required**: True
- **Type**: int
- **Default value**: `3`
- **Constraints**: The minimum value is `1`. The maximum value is `3`.
- **Description**: The maximum API version allowed to use.

### migration.full_scan_threads

- **Required**: True
- **Type**: int
- **Default value**: `2`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The number of threads used to scan the table.

### migration.full_scan_jobs

- **Required**: True
- **Type**: int
- **Default value**: `4`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The number of coroutines used to scan the table.

### migration.cursors_per_job

- **Required**: True
- **Type**: int
- **Default value**: `100`
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`.
- **Description**: The number of cursors each job will scan.
