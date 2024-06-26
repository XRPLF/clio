# <img src='./docs/img/xrpl-logo.svg' width='40' valign="top" /> Clio

[![Build status](https://github.com/XRPLF/clio/actions/workflows/build.yml/badge.svg?branch=develop)](https://github.com/XRPLF/clio/actions/workflows/build.yml?query=branch%3Adevelop)
[![Nightly release status](https://github.com/XRPLF/clio/actions/workflows/nightly.yml/badge.svg?branch=develop)](https://github.com/XRPLF/clio/actions/workflows/nightly.yml?query=branch%3Adevelop)
[![Clang-tidy checks status](https://github.com/XRPLF/clio/actions/workflows/clang-tidy.yml/badge.svg?branch=develop)](https://github.com/XRPLF/clio/actions/workflows/clang-tidy.yml?query=branch%3Adevelop)
[![Code coverage develop branch](https://codecov.io/gh/XRPLF/clio/branch/develop/graph/badge.svg?)](https://app.codecov.io/gh/XRPLF/clio)

Clio is an XRP Ledger API server optimized for RPC calls over WebSocket or JSON-RPC.
It stores validated historical ledger and transaction data in a more space efficient format, and uses up to 4 times less space than [rippled](https://github.com/XRPLF/rippled).

Clio can be configured to store data in [Apache Cassandra](https://cassandra.apache.org/_/index.html) or [ScyllaDB](https://www.scylladb.com/), enabling scalable read throughput.
Multiple Clio nodes can share access to the same dataset, which allows for a highly available cluster of Clio nodes without the need for redundant data storage or computation.

## 📡 Clio and `rippled`

Clio offers the full `rippled` API, with the caveat that Clio by default only returns validated data. This means that `ledger_index` defaults to `validated` instead of `current` for all requests. Other non-validated data, such as information about queued transactions, is also not returned.

Clio retrieves data from a designated group of `rippled` nodes instead of connecting to the peer-to-peer network.
For requests that require access to the peer-to-peer network, such as `fee` or `submit`, Clio automatically forwards the request to a `rippled` node and propagates the response back to the client. To access non-validated data for *any* request, simply add `ledger_index: "current"` to the request, and Clio will forward the request to `rippled`.

> [!NOTE]  
> Clio requires access to at least one `rippled` node, which can run on the same machine as Clio or separately.

## 📚 Learn more about Clio

Below are some useful docs to learn more about Clio.

**For Developers**:

- [How to build Clio](./docs/build-clio.md)
- [Metrics and static analysis](./docs/metrics-and-static-analysis.md)
- [Coverage report](./docs/coverage-report.md)

**For Operators**:

- [How to configure Clio and rippled](./docs/configure-clio.md)
- [How to run Clio](./docs/run-clio.md)
- [Logging](./docs/logging.md)
- [Troubleshooting guide](./docs/trouble_shooting.md)

**General reference material:**

- [API reference](https://xrpl.org/http-websocket-apis.html)
- [Developer docs](https://xrplf.github.io/clio)
- [Clio documentation](https://xrpl.org/the-clio-server.html#the-clio-server)

## 🆘 Help

Feel free to open an [issue](https://github.com/XRPLF/clio/issues) if you have a feature request or something doesn't work as expected.
If you have any questions about building, running, contributing, using Clio or any other, you could always start a new [discussion](https://github.com/XRPLF/clio/discussions).
