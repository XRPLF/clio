# Troubleshooting Guide
This guide will help you troubleshoot common issues of Clio.

## Can't connect to DB
If you see the error log message `Could not connect to Cassandra: No hosts available`, this means that Clio can't connect to the database. Check the following:
- Make sure the database is running at the specified address and port.
- Make sure the database is accessible from the machine where Clio is running.
You can use [cqlsh](https://pypi.org/project/cqlsh/) to check the connection to the database.
If you would like to run a local ScyllaDB, you can call:
```sh
docker run --rm -p 9042:9042 --name clio-scylla -d scylladb/scylla 
```

## Check the server status of Clio
To check if Clio is syncing with rippled:
```sh
curl -v -d '{"method":"server_info", "params":[{}]}' 127.0.0.1:51233|python3 -m json.tool|grep seq
```
If Clio is syncing with rippled, the `seq` value will be increasing.

## Clio fails to start
If you see the error log message `Failed to fetch ETL state from...`, this means the configured rippled node is not reachable. Check the following:
- Make sure the rippled node is running at the specified address and port.
- Make sure the rippled node is accessible from the machine where Clio is running.

If you would like to run Clio without an avaliable rippled node, you can add below setting to Clio's configuration file:
```
"allow_no_etl": true
```

## Clio is not added to secure_gateway in rippled's config
If you see the warning message `AsyncCallData is_unlimited is false.`, this means that Clio is not added to the `secure_gateway` of `port_grpc` session in the rippled configuration file. It will slow down the sync process. Please add Clio's IP to the `secure_gateway` in the rippled configuration file for both grpc and ws port.

## Clio is slow
To speed up the response time, Clio has a cache inside. However, cache can take time to warm up. If you see slow response time, you can firstly check if cache is still loading.
You can check the cache status by calling:
```sh
curl -v -d '{"method":"server_info", "params":[{}]}' 127.0.0.1:51233|python3 -m json.tool|grep is_full
curl -v -d '{"method":"server_info", "params":[{}]}' 127.0.0.1:51233|python3 -m json.tool|grep is_enabled
```
If `is_full` is false, it means the cache is still loading. Normally, the Clio can respond quicker if cache finishs loading. If `is_enabled` is false, it means the cache is disabled in the configuration file or there is data corruption in the database. 

## Receive error message `Too many requests`
If client sees the error message `Too many requests`, this means that the client is blocked by Clio's DosGuard protection. You may want to add the client's IP to the whitelist in the configuration file, Or update other your DosGuard settings.



