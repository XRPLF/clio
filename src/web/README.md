# Webserver subsystem

This folder contains all of the classes for running the webserver.

The webserver subsystem
- Handles JSON-RPC and websocket requests.
- Supports SSL if a cert and key file are specified in the config.
- Handles all types of requests on a single port.

Each request is handled asynchronously using boost asio.

Much of this code was originally copied from boost beast example code.
