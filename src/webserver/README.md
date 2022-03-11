This folder contains all of the classes for running the webserver.

The webserver handles JSON-RPC and websocket requests. The webserver supports SSL if a cert and key file are specified
in the config. The webserver handles all types of requests on a single port.

Each request is handled asynchronously using boost asio.

Much of this code was originally copied from boost beast example code.
