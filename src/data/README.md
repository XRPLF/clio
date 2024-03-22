# Backend

The backend of Clio is responsible for handling the proper reading and writing of past ledger data from and to a given database. Currently, Cassandra and ScyllaDB are the only supported databases that are production-ready.

To support additional database types, you can create new classes that implement the virtual methods in [BackendInterface.h](https://github.com/XRPLF/clio/blob/develop/src/data/BackendInterface.hpp). Then, leveraging the Factory Object Design Pattern, modify [BackendFactory.h](https://github.com/XRPLF/clio/blob/develop/src/data/BackendFactory.hpp) with logic that returns the new database interface if the relevant `type` is provided in Clio's configuration file.

## Data Model

The data model used by Clio to read and write ledger data is different from what `rippled` uses. `rippled` uses a novel data structure named [*SHAMap*](https://github.com/ripple/rippled/blob/master/src/ripple/shamap/README.md), which is a combination of a Merkle Tree and a Radix Trie. In a SHAMap, ledger objects are stored in the root vertices of the tree. Thus, looking up a record located at the leaf node of the SHAMap executes a tree search, where the path from the root node to the leaf node is the key of the record.

`rippled` nodes can also generate a proof-tree by forming a subtree with all the path nodes and their neighbors, which can then be used to prove the existence of the leaf node data to other `rippled` nodes. In short, the main purpose of the SHAMap data structure is to facilitate the fast validation of data integrity between different decentralized `rippled` nodes.

Since Clio only extracts past validated ledger data from a group of trusted `rippled` nodes, it can be safely assumed that the ledger data is correct without the need to validate with other nodes in the XRP peer-to-peer network. Because of this, Clio is able to use a flattened data model to store the past validated ledger data, which allows for direct record lookup with much faster constant time operations.

There are three main types of data in each XRP Ledger version:

- [Ledger Header](https://xrpl.org/ledger-header.html)

- [Transaction Set](https://xrpl.org/transaction-formats.html)

- [State Data](https://xrpl.org/ledger-object-types.html)

Due to the structural differences of the different types of databases, Clio may choose to represent these data types using a different schema for each unique database type.

### Keywords  

**Sequence**: A unique incrementing identification number used to label the different ledger versions.

**Hash**: The SHA512-half (calculate SHA512 and take the first 256 bits) hash of various ledger data like the entire ledger or specific ledger objects.

**Ledger Object**: The [binary-encoded](https://xrpl.org/serialization.html) STObject containing specific data (i.e. metadata, transaction data).  

**Metadata**: The data containing [detailed information](https://xrpl.org/transaction-metadata.html#transaction-metadata) of the outcome of a specific transaction, regardless of whether the transaction was successful.  

**Transaction data**: The data containing the [full details](https://xrpl.org/transaction-common-fields.html) of a specific transaction.  

**Object Index**: The pseudo-random unique identifier of a ledger object, created by hashing the data of the object.  

## Cassandra Implementation

Cassandra is a distributed wide-column NoSQL database designed to handle large data throughput with high availability and no single point of failure. By leveraging Cassandra, Clio is able to quickly and reliably scale up when needed simply by adding more Cassandra nodes to the Cassandra cluster configuration.

In Cassandra, Clio creates 9 tables to store the ledger data:

- `ledger_transactions`
- `transactions`
- `ledger_hashes`
- `ledger_range`
- `objects`
- `ledgers`
- `diff`
- `account_tx`
- `successor`

Their schemas and how they work are detailed in the following sections.

> **Note**: If you would like visually explore the data structure of the Cassandra database, run the Clio server with the database `type` configured as `cassandra` to fill ledger data from the `rippled` nodes into Cassandra. Then, use a GUI database management tool like [Datastax's Opcenter](https://docs.datastax.com/en/install/6.0/install/opscInstallOpsc.html) to interactively view it.

### ledger_transactions

```
CREATE TABLE clio.ledger_transactions (  
	ledger_sequence bigint,  # The sequence number of the ledger version
	hash blob,               # Hash of all the transactions on this ledger version
	PRIMARY KEY (ledger_sequence, hash)  
) WITH CLUSTERING ORDER BY (hash ASC) ...
```

This table stores the hashes of all transactions in a given ledger sequence and is sorted by the hash value in ascending order.

### transactions

```
CREATE TABLE clio.transactions (  
	hash blob PRIMARY KEY,   # The transaction hash
	date bigint,             # Date of the transaction
	ledger_sequence bigint,  # The sequence that the transaction was validated
	metadata blob,           # Metadata of the transaction
	transaction blob         # Data of the transaction
) ...
```

This table stores the full transaction and metadata of each ledger version with the transaction hash as the primary key.

To lookup all the transactions that were validated in a ledger version with sequence `n`, first get the all the transaction hashes in that ledger version by querying `SELECT * FROM ledger_transactions WHERE ledger_sequence = n;`. Then, iterate through the list of hashes and query `SELECT * FROM transactions WHERE hash = one_of_the_hash_from_the_list;` to get the detailed transaction data.  

### ledger_hashes

```
CREATE TABLE clio.ledger_hashes (
	hash blob PRIMARY KEY,  # Hash of entire ledger version's data
	sequence bigint         # The sequence of the ledger version
) ...
```

This table stores the hash of all ledger versions by their sequences. 

### ledger_range

```
CREATE TABLE clio.ledger_range (
	is_latest boolean PRIMARY KEY,  # Whether this sequence is the stopping range
	sequence bigint                 # The sequence number of the starting/stopping range
) ...
```

This table marks the range of ledger versions that is stored on this specific Cassandra node. Because of its nature, there are only two records in this table with `false` and `true` values for `is_latest`, marking the starting and ending sequence of the ledger range.

### objects

```
CREATE TABLE clio.objects (
	key blob,         # Object index of the object
	sequence bigint,  # The sequence this object was last updated
	object blob,      # Data of the object
	PRIMARY KEY (key, sequence)
) WITH CLUSTERING ORDER BY (sequence DESC) ...
```

The `objects` table stores the specific data of all objects that ever existed on the XRP network, even if they are deleted (which is represented with a special `0x` value). The records are ordered by descending sequence, where the newest validated ledger objects are at the top.

The table is updated when all data for a given ledger sequence has been written to the various tables in the database. For each ledger, many associated records are written to different tables. This table is used as a synchronization mechanism, to prevent the application from reading data from a ledger for which all data has not yet been fully written.

### ledgers

```
CREATE TABLE clio.ledgers (
	sequence bigint PRIMARY KEY,  # Sequence of the ledger version
	header blob                   # Data of the header
) ...
```

This table stores the ledger header data of specific ledger versions by their sequence.

### diff

```
CREATE TABLE clio.diff (
	seq bigint,  # Sequence of the ledger version
	key blob,    # Hash of changes in the ledger version
	PRIMARY KEY (seq, key)
) WITH CLUSTERING ORDER BY (key ASC) ...
```

This table stores the object index of all the changes in each ledger version.

### account_tx

```
CREATE TABLE clio.account_tx (
	account blob,
	seq_idx frozen<tuple<bigint, bigint>>,  # Tuple of (ledger_index, transaction_index)
	hash blob,                              # Hash of the transaction
	PRIMARY KEY (account, seq_idx)
) WITH CLUSTERING ORDER BY (seq_idx DESC) ...
```

This table stores the list of transactions affecting a given account. This includes transactions made by the account, as well as transactions received.

### successor

```
CREATE TABLE clio.successor (
	key blob,    # Object index
	seq bigint,  # The sequnce that this ledger object's predecessor and successor was updated
	next blob,   # Index of the next object that existed in this sequence
	PRIMARY KEY (key, seq)
) WITH CLUSTERING ORDER BY (seq ASC) ...
```

This table is the important backbone of how histories of ledger objects are stored in Cassandra. The `successor` table stores the object index of all ledger objects that were validated on the XRP network along with the ledger sequence that the object was updated on.

As each key is ordered by the sequence, which is achieved by tracing through the table with a specific sequence number, Clio can recreate a Linked List data structure that represents all the existing ledger objects at that ledger sequence. The special values of `0x00...00` and `0xFF...FF` are used to label the *head* and *tail* of the Linked List in the successor table.

The diagram below showcases how tracing through the same table, but with different sequence parameter filtering, can result in different Linked List data representing the corresponding past state of the ledger objects. A query like `SELECT * FROM successor WHERE key = ? AND seq <= n ORDER BY seq DESC LIMIT 1;` can effectively trace through the successor table and get the Linked List of a specific sequence `n`.

![Successor Table Trace Diagram](https://raw.githubusercontent.com/Shoukozumi/clio/9b2ea3efb6b164b02e9a5f0ef6717065a70f078c/src/backend/README.png)

> **Note**: The `diff` is `(DELETE 0x00...02, CREATE 0x00...03)` for `seq=1001` and `(CREATE 0x00...04)` for `seq=1002`, which is both accurately reflected with the Linked List trace.

In each new ledger version with sequence `n`, a ledger object `v` can either be **created**, **modified**, or **deleted**.

For all three of these operations, the procedure to update the successor table can be broken down into two steps:

 1. Trace through the Linked List of the previous sequence to find the ledger object `e` with the greatest object index smaller or equal than the `v`'s index. Save `e`'s `next` value (the index of the next ledger object) as `w`.

 2. If `v` is...
	 1. Being **created**, add two new records of `seq=n` with one being `e` pointing to `v`, and `v` pointing to `w` (Linked List insertion operation).
	 2. Being **modified**, do nothing.
	 3. Being **deleted**, add a record of `seq=n` with `e` pointing to `v`'s `next` value (Linked List deletion operation).

## NFT data model

In `rippled` NFTs are stored in `NFTokenPage` ledger objects. This object is implemented to save ledger space and has the property that it gives us O(1) lookup time for an NFT, assuming we know who owns the NFT at a particular ledger. However, if we do not know who owns the NFT at a specific ledger height we have no alternative but to scan the entire ledger in `rippled`. Because of this tradeoff, Clio implements a special NFT indexing data structure that allows Clio users to query NFTs quickly, while keeping rippled's space-saving optimizations.

### nf_tokens

```
CREATE TABLE clio.nf_tokens (
	token_id blob,         # The NFT's ID
	sequence bigint,       # Sequence of ledger version
	owner blob,            # The account ID of the owner of this NFT at this ledger
	is_burned boolean,     # True if token was burned in this ledger
	PRIMARY KEY (token_id, sequence)
) WITH CLUSTERING ORDER BY (sequence DESC) ...
```

This table indexes NFT IDs with their owner at a given ledger.

The example query below shows how you could search for the owner of token `N` at ledger `Y` and see whether the token was burned.

```
SELECT * FROM nf_tokens
WHERE token_id = N AND seq <= Y
ORDER BY seq DESC LIMIT 1;
```

If the token is burned, the owner field indicates the account that owned the token at the time it was burned; it does not indicate the person who burned the token, necessarily. If you need to determine who burned the token you can use the `nft_history` API, which will give you the `NFTokenBurn` transaction that burned this token, along with the account that submitted that transaction.

### issuer_nf_tokens_v2

```
CREATE TABLE clio.issuer_nf_tokens_v2 (
	issuer blob,       # The NFT issuer's account ID
	taxon bigint,      # The NFT's token taxon
	token_id blob,     # The NFT's ID
	PRIMARY KEY (issuer, taxon, token_id)
) WITH CLUSTERING ORDER BY (taxon ASC, token_id ASC) ...
```

This table indexes token IDs against their issuer and issuer/taxon
combination. This is useful for determining all the NFTs a specific account issued, or all the NFTs a specific account issued with a specific taxon. It is not useful to know all the NFTs with a given taxon while excluding issuer, since the meaning of a taxon is left to an issuer.

### nf_token_uris

```
CREATE TABLE clio.nf_token_uris (
	token_id blob,    # The NFT's ID
	sequence bigint,  # Sequence of ledger version
	uri blob,         # The NFT's URI
	PRIMARY KEY (token_id, sequence)
) WITH CLUSTERING ORDER BY (sequence DESC) ...
```

This table is used to store an NFT's URI. Without storing this here, we would need to traverse the NFT owner's entire set of NFTs to find the URI, again due to the way that NFTs are stored in `rippled`. Furthermore, instead of storing this in the `nf_tokens` table, we store it here to save space.

A given NFT will have only one entry in this table (see caveat below), and will be written to this table as soon as Clio sees the `NFTokenMint` transaction, or when Clio loads an `NFTokenPage` from the initial ledger it downloaded. However, the `nf_tokens` table is written to every time an NFT changes ownership, or if it is burned.

> **Why do we have to store the sequence?**
>
> Unfortunately there is an extreme edge case where a given NFT ID can be burned, and then re-minted with a different URI. This is extremely unlikely, and might be fixed in a future version of `rippled`. Currently, Clio handles this edge case by allowing the NFT ID to have a new URI assigned, without removing the prior URI.

### nf_token_transactions

```
CREATE TABLE clio.nf_token_transactions (
	token_id blob,                  # The NFT's ID
	seq_idx tuple<bigint, bigint>,  # Tuple of (ledger_index, transaction_index)
	hash blob,                      # Hash of the transaction
	PRIMARY KEY (token_id, seq_idx)
) WITH CLUSTERING ORDER BY (seq_idx DESC) ...
```

The `nf_token_transactions` table serves as the NFT counterpart to `account_tx`, inspired by the same motivations and fulfilling a similar role within this context. It drives the `nft_history` API.
