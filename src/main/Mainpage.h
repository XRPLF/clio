//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

/**
 * @mainpage Clio API server
 *
 * @section intro Introduction
 *
 * Clio is an XRP Ledger API server. Clio is optimized for RPC calls, over WebSocket or JSON-RPC.
 *
 * Validated historical ledger and transaction data are stored in a more space-efficient format, using up to 4 times
 * less space than rippled.
 *
 * Clio can be configured to store data in Apache Cassandra or ScyllaDB, allowing for scalable read throughput.
 * Multiple Clio nodes can share access to the same dataset, allowing for a highly available cluster of Clio nodes,
 * without the need for redundant data storage or computation.
 *
 * You can read more general information about Clio and its subsystems from the `Related Pages` section.
 *
 * @section Develop
 *
 * As you prepare to develop code for Clio, please be sure you are aware of our current
 * <A HREF="https://github.com/XRPLF/clio/blob/develop/CONTRIBUTING.md">Contribution guidelines</A>.
 *
 * Read `rpc/README.md` carefully to know more about writing your own handlers for
 * Clio.
 */
