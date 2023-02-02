# Clio Backend
## Background
The backend of Clio is responsible for handling the proper reading and writing of past ledger data from and to a given database. As of right now, Cassandra and ScyllaDB are the only supported databases that are production-ready. Support for database types can be easily extended by creating new implementations which implements the virtual methods of `BackendInterface.h`. Then, use the Factory Object Design Pattern to simply add logic statements to `BackendFactory.h` that return the new database interface for a specific `type` in Clio's configuration file. 

## Data Model
The data model used by Clio to read and write ledger data is different from what Rippled uses. Rippled uses a novel data structure named [*SHAMap*](https://github.com/ripple/rippled/blob/master/src/ripple/shamap/README.md), which is a combination of a Merkle Tree and a Radix Trie. In a SHAMap, ledger objects are stored in the root vertices of the tree. Thus, looking up a record located at the leaf node of the SHAMap executes a tree search, where the path from the root node to the leaf node is the key of the record. Rippled nodes can also generate a proof-tree by forming a subtree with all the path nodes and their neighbors, which can then be used to prove the existnce of the leaf node data to other Rippled nodes. In short, the main purpose of the SHAMap data structure is to facilitate the fast validation of data integrity between different decentralized Rippled nodes.

Since Clio only extracts past validated ledger data from a group of trusted Rippled nodes, it can be safely assumed that these ledger data are correct without the need to validate with other  nodes in the XRP peer-to-peer network. Because of this, Clio is able to use a flattened data model to store the past validated ledger data, which allows for direct record lookup with much faster constant time operations. 

There are three main types of data in each XRP ledger version, they are [Ledger Header](https://xrpl.org/ledger-header.html), [Transaction Set](https://xrpl.org/transaction-formats.html) and [State Data](https://xrpl.org/ledger-object-types.html). Due to the structural differences of the different types of databases, Clio may choose to represent these data using a different schema for each unique database type. 

**Keywords**  
*Sequence*: A unique incrementing identification number used to label the different ledger versions.  
*Hash*: The SHA512-half (calculate SHA512 and take the first 256 bits) hash of various ledger data like the entire ledger or specific ledger objects.
*Ledger Object*: The [binary-encoded](https://xrpl.org/serialization.html) STObject containing specific data (i.e. metadata, transaction data).  
*Metadata*: The data containing [detailed information](https://xrpl.org/transaction-metadata.html#transaction-metadata) of the outcome of a specific transaction, regardless of whether the transaction was successful.  
*Transaction data*: The data containing the [full details](https://xrpl.org/transaction-common-fields.html) of a specific transaction.  
*Object Index*: The pseudo-random unique identifier of a ledger object, created by hashing the data of the object.  

## Cassandra Implementation
Cassandra is a distributed wide-column NoSQL database designed to handle large data throughput with high availability and no single point of failure. By leveraging Cassandra, Clio will be able to quickly and reliably scale up when needed simply by adding more Cassandra nodes to the Cassandra cluster configuration.

In Cassandra, Clio will be creating 9 tables to store the ledger data, they are `ledger_transactions`, `transactions`, `ledger_hashes`, `ledger_range`, `objects`, `ledgers`, `diff`, `account_tx`, and `successor`.  Their schemas and how they work are detailed below.

*Note, if you would like visually explore the data structure of the Cassandra database, you can first run Clio server with database `type` configured as `cassandra` to fill ledger data from Rippled nodes into Cassandra, then use a GUI database management tool like [Datastax's Opcenter](https://docs.datastax.com/en/install/6.0/install/opscInstallOpsc.html) to interactively view it.* 


### `ledger_transactions`
```
CREATE TABLE clio.ledger_transactions (  
	ledger_sequence bigint,  # The sequence number of the ledger version
	hash blob,               # Hash of all the transactions on this ledger version
	PRIMARY KEY (ledger_sequence, hash)  
) WITH CLUSTERING ORDER BY (hash ASC) ...
 ```
This table stores the hashes of all transactions in a given ledger sequence ordered by the hash value in ascending order. 

### `transactions`
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

To look up all the transactions that were validated in a ledger version with sequence `n`, one can first get the all the transaction hashes in that ledger version by querying `SELECT * FROM ledger_transactions WHERE ledger_sequence = n;`. Then, iterate through the list of hashes and query `SELECT * FROM transactions WHERE hash = one_of_the_hash_from_the_list;` to get the detailed transaction data.  

### `ledger_hashes`
```
CREATE TABLE clio.ledger_hashes (
	hash blob PRIMARY KEY,  # Hash of entire ledger version's data
	sequence bigint         # The sequence of the ledger version
) ...
 ```
This table stores the hash of all ledger versions by their sequences. 
### `ledger_range`
```
CREATE TABLE clio.ledger_range (
	is_latest boolean PRIMARY KEY,  # Whether this sequence is the stopping range
	sequence bigint                 # The sequence number of the starting/stopping range
) ...
 ```
This table marks the range of ledger versions that is stored on this specific Cassandra node. Because of its nature, there are only two records in this table with `false` and `true` values for `is_latest`, marking the starting and ending sequence of the ledger range. 

### `objects`
```
CREATE TABLE clio.objects (
	key blob,         # Object index of the object
	sequence bigint,  # The sequence this object was last updated
	object blob,      # Data of the object
	PRIMARY KEY (key, sequence)
) WITH CLUSTERING ORDER BY (sequence DESC) ...
 ```
This table stores the specific data of all objects that ever existed on the XRP network, even if they are deleted (which is represented with a special `0x` value). The records are ordered by descending sequence, where the newest validated ledger objects are at the top. 

This table is updated when all data for a given ledger sequence has been written to the various tables in the database. For each ledger, many associated records are written to different tables. This table is used as a synchronization mechanism, to prevent the application from reading data from a ledger for which all data has not yet been fully written.

### `ledgers`
```
CREATE TABLE clio.ledgers (
	sequence bigint PRIMARY KEY,  # Sequence of the ledger version
	header blob                   # Data of the header
) ...
 ```
This table stores the ledger header data of specific ledger versions by their sequence.

### `diff`
```
CREATE TABLE clio.diff (
	seq bigint,  # Sequence of the ledger version
	key blob,    # Hash of changes in the ledger version
	PRIMARY KEY (seq, key)
) WITH CLUSTERING ORDER BY (key ASC) ...
 ```
This table stores the object index of all the changes in each ledger version.

### `account_tx`
```
CREATE TABLE clio.account_tx (
	account blob,
	seq_idx frozen<tuple<bigint, bigint>>,  # Tuple of (ledger_index, transaction_index)
	hash blob,                              # Hash of the transaction
	PRIMARY KEY (account, seq_idx)
) WITH CLUSTERING ORDER BY (seq_idx DESC) ...
 ```
This table stores the list of transactions affecting a given account. This includes transactions made by the account, as well as transactions received.


### `successor`
```
CREATE TABLE clio.successor (
	key blob,    # Object index
	seq bigint,  # The sequnce that this ledger object's predecessor and successor was updated
	next blob,   # Index of the next object that existed in this sequence
	PRIMARY KEY (key, seq)
) WITH CLUSTERING ORDER BY (seq ASC) ...
  ```
This table is the important backbone of how histories of ledger objects are stored in Cassandra. The successor table stores the object index of all ledger objects that were validated on the XRP network along with the ledger sequence that the object was upated on. Due to the unique nature of the table with each key being ordered by the sequence, by tracing through the table with a specific sequence number, Clio can recreate a Linked List data structure that represents all the existing ledger object at that ledger sequence. The special value of `0x00...00` and `0xFF...FF` are used to label the head and tail of the Linked List in the successor table. The diagram below showcases how tracing through the same table but with different sequence parameter filtering can result in different Linked List data representing the corresponding past state of the ledger objects. A query like `SELECT * FROM successor WHERE key = ? AND seq <= n ORDER BY seq DESC LIMIT 1;` can effectively trace through the successor table and get the Linked List of a specific sequence `n`.

![Successor Table Trace Diagram](https://raw.githubusercontent.com/Shoukozumi/clio/9b2ea3efb6b164b02e9a5f0ef6717065a70f078c/src/backend/README.png)
*P.S.: The `diff` is `(DELETE 0x00...02, CREATE 0x00...03)` for `seq=1001` and `(CREATE 0x00...04)` for `seq=1002`, which is both accurately reflected with the Linked List trace*

In each new ledger version with sequence `n`, a ledger object `v` can either be **created**, **modified**, or **deleted**. For all three of these operations, the procedure to update the successor table can be broken down in to two steps: 
 1. Trace through the Linked List of the previous sequence to to find the ledger object `e` with the greatest object index smaller or equal than the `v`'s index. Save `e`'s `next` value (the index of the next ledger object) as `w`.
 2. If `v` is...
	 1. Being **created**, add two new records of `seq=n` with one being `e` pointing to `v`, and `v` pointing to `w` (Linked List insertion operation).
	 2. Being **modified**, do nothing.
	 3. Being **deleted**, add a record of `seq=n` with `e` pointing to `v`'s `next` value (Linked List deletion operation).
