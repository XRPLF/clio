The data model used by clio is different than that used by rippled.
rippled uses what is known as a SHAMap, which is a tree structure, with
actual ledger and transaction data at the leaves of the tree. Looking up a record
is a tree traversal, where the key is used to determine the path to the proper
leaf node. The path from root to leaf is used as a proof-tree on the p2p network,
where nodes can prove that a piece of data is present in a ledger by sending
the path from root to leaf. Other nodes can verify this path and be certain
that the data does actually exist in the ledger in question.

clio instead flattens the data model, so lookups are O(1). This results in time
and space savings. This is possible because clio does not participate in the peer
to peer protocol, and thus does not need to verify any data. clio fully trusts the
rippled nodes that are being used as a data source.

clio uses certain features of database query languages to make this happen. Many
databases provide the necessary features to implement the clio data model. At the
time of writing, the data model is implemented in PostgreSQL and CQL (the query
language used by Apache Cassandra and ScyllaDB).

The below examples are a sort of pseudo query language

## Ledgers

We store ledger headers in a ledgers table. In PostgreSQL, we store
the headers in their deserialized form, so we can look up by sequence or hash.

In Cassandra, we store the headers as blobs. The primary table maps a ledger sequence
to the blob, and a secondary table maps a ledger hash to a ledger sequence.

## Transactions
Transactions are stored in a very basic table, with a schema like so:

```
CREATE TABLE transactions (
hash blob,
ledger_sequence int,
transaction blob,
PRIMARY KEY(hash))
```
The primary key is the hash.

A common query pattern is fetching all transactions in a ledger. In PostgreSQL,
nothing special is needed for this. We just query:
```
SELECT * FROM transactions WHERE ledger_sequence = s;
```
Cassandra doesn't handle queries like this well, since `ledger_sequence` is not
the primary key, so we use a second table that maps a ledger sequence number
to all of the hashes in that ledger:

```
CREATE TABLE transaction_hashes (
ledger_sequence int,
hash blob,
PRIMARY KEY(ledger_sequence, blob))
```
This table uses a compound primary key, so we can have multiple records with
the same ledger sequence but different hash. Looking up all of the transactions
in a given ledger then requires querying the transaction_hashes table to get the hashes of
all of the transactions in the ledger, and then using those hashes to query the
transactions table. Sometimes we only want the hashes though.

## Ledger data

Ledger data is more complicated than transaction data. Objects have different versions,
where applying transactions in a particular ledger changes an object with a given
key. A basic example is an account root object: the balance changes with every
transaction sent or received, though the key (object ID) for this object remains the same.

Ledger data then is modeled like so:

```
CREATE TABLE objects (
id blob,
ledger_sequence int,
object blob,
PRIMARY KEY(key,ledger_sequence))
```

The `objects` table has a compound primary key. This is essential. Looking up
a ledger object as of a given ledger then is just:
```
SELECT object FROM objects WHERE id = ? and ledger_sequence <= ?
    ORDER BY ledger_sequence DESC LIMIT 1;
```
This gives us the most recent ledger object written at or before a specified ledger.

When a ledger object is deleted, we write a record where `object` is just an empty blob.

### Next
Generally RPCs that read ledger data will just use the above query pattern. However,
a few RPCs (`book_offers` and `ledger_data`) make use of a certain tree operation
called `successor`, which takes in an object id and ledger sequence, and returns
the id of the successor object in the ledger. This is the object in the ledger with the smallest id
greater than the input id.

This problem is quite difficult for clio's data model, since computing this
generally requires the inner nodes of the tree, which clio doesn't store. A naive
way to do this with PostgreSQL is like so:
```
SELECT * FROM objects WHERE id > ? AND ledger_sequence <= s ORDER BY id ASC, ledger_sequence DESC LIMIT 1;
```
This query is not really possible with Cassandra, unless you use ALLOW FILTERING, which
is an anti pattern (for good reason!). It would require contacting basically every node
in the entire cluster.

But even with Postgres, this query is not scalable. Why? Consider what the query
is doing at the database level. The database starts at the input id, and begins scanning
the table in ascending order of id. It needs to skip over any records that don't actually
exist in the desired ledger, which are objects that have been deleted, or objects that
were created later. As ledger history grows, this query skips over more and more records,
which results in the query taking longer and longer. The time this query takes grows
unbounded then, as ledger history just keeps growing. With under a million ledgers, this
query is usable, but as we approach 10 million ledgers are more, the query starts to become very slow.

To alleviate this issue, the data model uses a checkpointing method. We create a second
table called keys, like so:
```
CREATE TABLE keys (
ledger_sequence int,
id blob,
PRIMARY KEY(ledger_sequence, id)
)
```
However, this table does not have an entry for every ledger sequence. Instead,
this table has an entry for rougly every 1 million ledgers. We call these ledgers
flag ledgers. For each flag ledger, the keys table contains every object id in that
ledger, as well as every object id that existed in any ledger between the last flag
ledger and this one. This is a lot of keys, but not every key that ever existed (which
is what the naive attempt at implementing successor was iterating over). In this manner,
the performance is bounded. If we wanted to increase the performance of the successor operation,
we can increase the frequency of flag ledgers. However, this will use more space. 1 million
was chosen as a reasonable tradeoff to bound the performance, but not use too much space,
especially since this is only needed for two RPC calls.

We write to this table every ledger, for each new key. However, we also need to handle
keys that existed in the previous flag ledger. To do that, at each flag ledger, we
iterate through the previous flag ledger, and write any keys that are still present
in the new flag ledger. This is done asynchronously.

## Account Transactions
rippled offers a RPC called `account_tx`. This RPC returns all transactions that
affect a given account, and allows users to page backwards or forwards in time.
Generally, this is a modeled with a table like so:
```
CREATE TABLE account_tx (
account blob,
ledger_sequence int,
transaction_index int,
hash blob,
PRIMARY KEY(account,ledger_sequence,transaction_index))
```

An example of looking up from this table going backwards in time is:
```
SELECT hash FROM account_tx WHERE account = ? 
    AND ledger_sequence <= ? and transaction_index <= ? 
    ORDER BY ledger_sequence DESC, transaction_index DESC;
```

This query returns the hashes, and then we use those hashes to read from the 
transactions table.

## Comments
There are various nuances around how these data models are tuned and optimized
for each database implementation. Cassandra and PostgreSQL are very different,
so some slight modifications are needed. However, the general model outlined here
is implemented by both databases, and when adding a new database, this general model
should be followed, unless there is a good reason not to. Generally, a database will be
decently similar to either PostgreSQL or Cassandra, so using those as a basis should
be sufficient.

Whatever database is used, clio requires strong consistency, and durability. For this
reason, any replication strategy needs to maintain strong consistency.

# Developing against `rippled` in standalone mode

If you wish you develop against a `rippled` instance running in standalone
mode there are a few quirks of both clio and rippled you need to keep in mind.
You must:

1. Advance the `rippled` ledger to at least ledger 256
2. Wait 10 minutes before first starting clio against this standalone node.
