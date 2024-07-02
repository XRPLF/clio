# Clio official docker image

[Clio](https://github.com/XRPLF/clio) is an XRP Ledger API server optimized for RPC calls over WebSocket or JSON-RPC.
It stores validated historical ledger and transaction data in a space efficient format.

This image contains `clio_server` binary allowing users to run Clio easily.

## Clio configuration file

Please note that while Clio requires a configuration file, this image doesn't include any default config.
Your configuration file should be mounted under the path `/opt/clio/etc/config.json`.
Clio repository provides an [example](https://github.com/XRPLF/clio/blob/develop/docs/examples/config/example-config.json) of the configuration file.

Config file recommendations:
- Set `log_to_console` to `false` if you want to avoid logs being written to `stdout`.
- Set `log_directory` to `/opt/clio/log` to store logs in a volume.

## Usage

The following command can be used to run Clio in docker (assuming server's port is `51233` in your config):
```bash
docker run -d -v <path to your config.json>:/opt/clio/etc/config.json -v <path to store logs>:/opt/clio/log -p 51233:51233 rippleci/clio
```
