# Clio Config Description
This file lists all Clio Configuration definitions in detail.

## Configuration Details

### Key: database.type
- **Required**: True
- **Type**: string
- **Default value**: cassandra
- **Constraints**: The value must be one of the following: `cassandra`
 -   **Description**: Type of database to use. We currently support Cassandra and Scylladb. We default to Scylladb.
### Key: database.cassandra.contact_points
- **Required**: True
- **Type**: string
- **Default value**: localhost
- **Constraints**: None
 -   **Description**: A list of IP addresses or hostnames of the initial nodes (Cassandra/Scylladb cluster nodes) that the client will connect to when establishing a connection with the database. If you're running locally, it should be 'localhost' or 127.0.0.1
### Key: database.cassandra.secure_connect_bundle
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
 -   **Description**: Configuration file that contains the necessary security credentials and connection details for securely connecting to a Cassandra database cluster.
### Key: database.cassandra.port
- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `65535
 -   **Description**: Port number to connect to the database.
### Key: database.cassandra.keyspace
- **Required**: True
- **Type**: string
- **Default value**: clio
- **Constraints**: None
 -   **Description**: Keyspace to use for the database.
### Key: database.cassandra.replication_factor
- **Required**: True
- **Type**: int
- **Default value**: 3
- **Constraints**: The minimum value is `0`. The maximum value is `65535`
 -   **Description**: Number of replicated nodes for Scylladb. Visit this link for more details : https://university.scylladb.com/courses/scylla-essentials-overview/lessons/high-availability/topic/fault-tolerance-replication-factor/ 
### Key: database.cassandra.table_prefix
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
 -   **Description**: Prefix for Database table names.
### Key: database.cassandra.max_write_requests_outstanding
- **Required**: True
- **Type**: int
- **Default value**: 10000
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: Maximum number of outstanding write requests. Write requests are api calls that write to database 
### Key: database.cassandra.max_read_requests_outstanding
- **Required**: True
- **Type**: int
- **Default value**: 100000
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: Maximum number of outstanding read requests, which reads from database
### Key: database.cassandra.threads
- **Required**: True
- **Type**: int
- **Default value**: The number of available CPU cores.
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: Number of threads that will be used for database operations.
### Key: database.cassandra.core_connections_per_host
- **Required**: True
- **Type**: int
- **Default value**: 1
- **Constraints**: The minimum value is `0`. The maximum value is `65535`
 -   **Description**: Number of core connections per host for Cassandra.
### Key: database.cassandra.queue_size_io
- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `0`. The maximum value is `65535`
 -   **Description**: Queue size for I/O operations in Cassandra.
### Key: database.cassandra.write_batch_size
- **Required**: True
- **Type**: int
- **Default value**: 20
- **Constraints**: The minimum value is `0`. The maximum value is `65535`
 -   **Description**: Batch size for write operations in Cassandra.
### Key: database.cassandra.connect_timeout
- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: The maximum amount of time in seconds the system will wait for a connection to be successfully established with the database.
### Key: database.cassandra.request_timeout
- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: The maximum amount of time in seconds the system will wait for a request to be fetched from database.
### Key: database.cassandra.username
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
 -   **Description**: The username used for authenticating with the database.
### Key: database.cassandra.password
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
 -   **Description**: The password used for authenticating with the database.
### Key: database.cassandra.certfile
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
 -   **Description**: The path to the SSL/TLS certificate file used to establish a secure connection between the client and the Cassandra database.
### Key: allow_no_etl
- **Required**: True
- **Type**: boolean
- **Default value**: True
- **Constraints**: None
 -   **Description**: If True, no ETL nodes will run with Clio.
### Key: etl_sources.[].ip
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: The value must be a valid IP address
 -   **Description**: IP address of the ETL source.
### Key: etl_sources.[].ws_port
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `65535
 -   **Description**: WebSocket port of the ETL source.
### Key: etl_sources.[].grpc_port
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `65535
 -   **Description**: gRPC port of the ETL source.
### Key: forwarding.cache_timeout
- **Required**: True
- **Type**: double
- **Default value**: 0
- **Constraints**: The value must be a positive double number
 -   **Description**: Timeout duration for the forwarding cache used in Rippled communication.
### Key: forwarding.request_timeout
- **Required**: True
- **Type**: double
- **Default value**: 10
- **Constraints**: The value must be a positive double number
 -   **Description**: Timeout duration for the forwarding request used in Rippled communication.
### Key: rpc.cache_timeout
- **Required**: True
- **Type**: double
- **Default value**: 0
- **Constraints**: The value must be a positive double number
 -   **Description**: Timeout duration for RPC requests.
### Key: num_markers
- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `256`
 -   **Description**: The number of markers is the number of coroutines to download the initial ledger
### Key: dos_guard.whitelist.[]
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
 -   **Description**: List of IP addresses to whitelist for DOS protection.
### Key: dos_guard.max_fetches
- **Required**: True
- **Type**: int
- **Default value**: 1000000
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: Maximum number of fetch operations allowed by DOS guard.
### Key: dos_guard.max_connections
- **Required**: True
- **Type**: int
- **Default value**: 20
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: Maximum number of concurrent connections allowed by DOS guard.
### Key: dos_guard.max_requests
- **Required**: True
- **Type**: int
- **Default value**: 20
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: Maximum number of requests allowed by DOS guard.
### Key: dos_guard.sweep_interval
- **Required**: True
- **Type**: double
- **Default value**: 1
- **Constraints**: The value must be a positive double number
 -   **Description**: Interval in seconds for DOS guard to sweep/clear its state.
### Key: workers
- **Required**: True
- **Type**: int
- **Default value**: The number of available CPU cores.
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: Number of threads to process RPC requests.
### Key: server.ip
- **Required**: True
- **Type**: string
- **Default value**: None
- **Constraints**: The value must be a valid IP address
 -   **Description**: IP address of the Clio HTTP server.
### Key: server.port
- **Required**: True
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `1`. The maximum value is `65535
 -   **Description**: Port number of the Clio HTTP server.
### Key: server.max_queue_size
- **Required**: True
- **Type**: int
- **Default value**: 0
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: Maximum size of the server's request queue. Value of 0 is no limit.
### Key: server.local_admin
- **Required**: False
- **Type**: boolean
- **Default value**: None
- **Constraints**: None
 -   **Description**: Indicates if the server should run with admin privileges. Only one of local_admin or admin_password can be set.
### Key: server.admin_password
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
 -   **Description**: Password for Clio admin-only APIs. Only one of local_admin or admin_password can be set.
### Key: server.processing_policy
- **Required**: True
- **Type**: string
- **Default value**: parallel
- **Constraints**: The value must be one of the following: `parallel, sequent`
 -   **Description**: Could be "sequent" or "parallel". For the sequent policy, requests from a single client 
        connection are processed one by one, with the next request read only after the previous one is processed. For the parallel policy, Clio will accept
         all requests and process them in parallel, sending a reply for each request as soon as it is ready.
### Key: server.parallel_requests_limit
- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `0`. The maximum value is `65535`
 -   **Description**: Optional parameter, used only if processing_strategy `parallel`. It limits the number of requests for a single client connection that are processed in parallel. If not specified, the limit is infinite.
### Key: server.ws_max_sending_queue_size
- **Required**: True
- **Type**: int
- **Default value**: 1500
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: Maximum size of the websocket sending queue.
### Key: prometheus.enabled
- **Required**: True
- **Type**: boolean
- **Default value**: False
- **Constraints**: None
 -   **Description**: Enable or disable Prometheus metrics.
### Key: prometheus.compress_reply
- **Required**: True
- **Type**: boolean
- **Default value**: False
- **Constraints**: None
 -   **Description**: Enable or disable compression of Prometheus responses.
### Key: io_threads
- **Required**: True
- **Type**: int
- **Default value**: 2
- **Constraints**: The minimum value is `1`. The maximum value is `65535`
 -   **Description**: Number of I/O threads. Value cannot be less than 1
### Key: subscription_workers
- **Required**: True
- **Type**: int
- **Default value**: 1
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: The number of worker threads or processes that are responsible for managing and processing subscription-based tasks from rippled
### Key: graceful_period
- **Required**: True
- **Type**: double
- **Default value**: 10
- **Constraints**: The value must be a positive double number
 -   **Description**: Number of milliseconds server will wait to shutdown gracefully.
### Key: cache.num_diffs
- **Required**: True
- **Type**: int
- **Default value**: 32
- **Constraints**: The minimum value is `0`. The maximum value is `65535`
 -   **Description**: Number of diffs to cache. For more info, consult readme.md in etc
### Key: cache.num_markers
- **Required**: True
- **Type**: int
- **Default value**: 48
- **Constraints**: The minimum value is `0`. The maximum value is `65535`
 -   **Description**: Number of markers to cache.
### Key: cache.num_cursors_from_diff
- **Required**: True
- **Type**: int
- **Default value**: 0
- **Constraints**: The minimum value is `0`. The maximum value is `65535`
 -   **Description**: Num of cursors that are different.
### Key: cache.num_cursors_from_account
- **Required**: True
- **Type**: int
- **Default value**: 0
- **Constraints**: The minimum value is `0`. The maximum value is `65535`
 -   **Description**: Number of cursors from an account.
### Key: cache.page_fetch_size
- **Required**: True
- **Type**: int
- **Default value**: 512
- **Constraints**: The minimum value is `0`. The maximum value is `65535`
 -   **Description**: Page fetch size for cache operations.
### Key: cache.load
- **Required**: True
- **Type**: string
- **Default value**: async
- **Constraints**: The value must be one of the following: `sync, async, none`
 -   **Description**: Cache loading strategy ('sync' or 'async').
### Key: log_channels.[].channel
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: The value must be one of the following: `General, WebServer, Backend, RPC, ETL, Subscriptions, Performance, Migration`
 -   **Description**: Name of the log channel.'RPC', 'ETL', and 'Performance'
### Key: log_channels.[].log_level
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: The value must be one of the following: `trace, debug, info, warning, error, fatal, count`
 -   **Description**: Log level for the specific log channel.`warning`, `error`, `fatal`
### Key: log_level
- **Required**: True
- **Type**: string
- **Default value**: info
- **Constraints**: The value must be one of the following: `trace, debug, info, warning, error, fatal, count`
 -   **Description**: General logging level of Clio. This level will be applied to all log channels that do not have an explicitly defined logging level.
### Key: log_format
- **Required**: True
- **Type**: string
- **Default value**: %TimeStamp% (%SourceLocation%) [%ThreadID%] %Channel%:%Severity% %Message%
- **Constraints**: None
 -   **Description**: Format string for log messages.
### Key: log_to_console
- **Required**: True
- **Type**: boolean
- **Default value**: True
- **Constraints**: None
 -   **Description**: Enable or disable logging to console.
### Key: log_directory
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
 -   **Description**: Directory path for log files.
### Key: log_rotation_size
- **Required**: True
- **Type**: int
- **Default value**: 2048
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`
 -   **Description**: Log rotation size in megabytes. When the log file reaches this particular size, a new log file starts.
### Key: log_directory_max_size
- **Required**: True
- **Type**: int
- **Default value**: 51200
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`
 -   **Description**: Maximum size of the log directory in megabytes.
### Key: log_rotation_hour_interval
- **Required**: True
- **Type**: int
- **Default value**: 12
- **Constraints**: The minimum value is `1`. The maximum value is `4294967295`
 -   **Description**: Interval in hours for log rotation. If the current log file reaches this value in logging, a new log file starts.
### Key: log_tag_style
- **Required**: True
- **Type**: string
- **Default value**: none
- **Constraints**: The value must be one of the following: `int, uint, null, none, uuid`
 -   **Description**: Style for log tags.
### Key: extractor_threads
- **Required**: True
- **Type**: int
- **Default value**: 1
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: Number of extractor threads.
### Key: read_only
- **Required**: True
- **Type**: boolean
- **Default value**: True
- **Constraints**: None
 -   **Description**: Indicates if the server should have read-only privileges.
### Key: txn_threshold
- **Required**: True
- **Type**: int
- **Default value**: 0
- **Constraints**: The minimum value is `0`. The maximum value is `65535`
 -   **Description**: Transaction threshold value.
### Key: start_sequence
- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: Starting ledger index.
### Key: finish_sequence
- **Required**: False
- **Type**: int
- **Default value**: None
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: Ending ledger index.
### Key: ssl_cert_file
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
 -   **Description**: Path to the SSL certificate file.
### Key: ssl_key_file
- **Required**: False
- **Type**: string
- **Default value**: None
- **Constraints**: None
 -   **Description**: Path to the SSL key file.
### Key: api_version.default
- **Required**: True
- **Type**: int
- **Default value**: 1
- **Constraints**: The minimum value is `1`. The maximum value is `3`
 -   **Description**: Default API version Clio will run on.
### Key: api_version.min
- **Required**: True
- **Type**: int
- **Default value**: 1
- **Constraints**: The minimum value is `1`. The maximum value is `3`
 -   **Description**: Minimum API version.
### Key: api_version.max
- **Required**: True
- **Type**: int
- **Default value**: 3
- **Constraints**: The minimum value is `1`. The maximum value is `3`
 -   **Description**: Maximum API version.
### Key: migration.full_scan_threads
- **Required**: True
- **Type**: int
- **Default value**: 2
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: The number of threads used to scan the table.
### Key: migration.full_scan_jobs
- **Required**: True
- **Type**: int
- **Default value**: 4
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: The number of coroutines used to scan the table.
### Key: migration.cursors_per_job
- **Required**: True
- **Type**: int
- **Default value**: 100
- **Constraints**: The minimum value is `0`. The maximum value is `4294967295`
 -   **Description**: The number of cursors each coroutine will scan.

