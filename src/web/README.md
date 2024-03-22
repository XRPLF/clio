# Web server subsystem

This folder contains all of the classes for running the web server.

The web server subsystem:

- Handles JSON-RPC and websocket requests.

- Supports SSL if a cert and key file are specified in the config.

- Handles all types of requests on a single port.

Each request is handled asynchronously using [Boost Asio](https://www.boost.org/doc/libs/1_82_0/doc/html/boost_asio.html).

Much of this code was originally copied from Boost beast example code.
