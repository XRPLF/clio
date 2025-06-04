# Web Server Subsystem

This folder contains all of the classes for running Clio's web server.

## Overview

The web server subsystem:

- Handles JSON-RPC requests over HTTP and WebSocket connections
- Supports SSL/TLS encryption if certificate and key files are specified in the config
- Processes all types of requests on a single port
- Implements asynchronous request handling using [Boost Asio](https://www.boost.org/doc/libs/1_83_0/doc/html/boost_asio.html)
- Provides request rate limiting through a built-in Denial-of-Service (DoS) Guard mechanism
- Supports both sequential and parallel request processing policies

## Key Components

### Core Components

- **Server** (`Server.hpp/cpp`): The main web server class that manages connections and routes requests
- **Connection** (`Connection.hpp/cpp`): Represents a client connection and provides an abstraction layer over HTTP and WebSocket connections
- **Request/Response** (`Request.hpp/cpp`, `Response.hpp/cpp`): Classes for handling HTTP requests and responses
- **MessageHandler** (`MessageHandler.hpp`): An interface for handling different types of messages (e.g., HTTP GET/POST, WebSocket)
- **RPCServerHandler** (`RPCServerHandler.hpp`): Handles RPC requests and integrates with the RPC engine

### Connection Processing

- **ConnectionHandler** (`impl/ConnectionHandler.hpp/cpp`): Manages the lifecycle of connections and processes requests
- **ProcessingPolicy** (`ProcessingPolicy.hpp`): Defines whether requests are processed sequentially or in parallel
- **HttpConnection/WsConnection** (`impl/HttpConnection.hpp`, `impl/WsConnection.hpp`): Concrete implementations for HTTP and WebSocket connections

### Security Features

- **DOSGuard** (`dosguard/DOSGuard.hpp/cpp`): Denial-of-Service protection system that implements rate limiting
- **IntervalSweepHandler** (`dosguard/IntervalSweepHandler.hpp/cpp`): Periodically clears DoS guard state
- **WhitelistHandler** (`dosguard/WhitelistHandler.hpp/cpp`): Manages IP address whitelisting for bypass of rate limits
- **AdminVerificationStrategy** (`AdminVerificationStrategy.hpp/cpp`): Handles verification of admin privileges

### Subscription

- **SubscriptionContext** (`SubscriptionContext.hpp/cpp`): Manages WebSocket subscriptions for streaming updates

## Architecture

The server design uses the following patterns:

- **RAII**: Resource management through C++ RAII principles
- **Dependency Injection**: Components accept their dependencies through constructor parameters
- **Interface-based design**: Components depend on interfaces rather than concrete implementations
- **Asynchronous programming**: Uses Boost Asio for non-blocking I/O operations with coroutines

Each incoming request is handled asynchronously, with the processing being dispatched to appropriate handlers based on the request type (GET, POST, WebSocket). The server supports both secure (SSL/TLS) and non-secure connections based on configuration.

## SSL Support

The server creates an SSL context if certificate and key files are specified in the configuration. When SSL is enabled, all connections are encrypted.

## Request Flow

1. Client connects to the server
2. Server performs security checks (e.g., DoS Guard, admin verification if needed)
3. Server reads the request asynchronously
4. Request is routed to appropriate handler based on HTTP method and target
5. Handler processes the request and generates a response
6. Response is sent back to the client
7. For persistent connections, the server returns to step 3
8. When the client disconnects or an error occurs, the connection is closed
