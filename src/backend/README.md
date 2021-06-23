The backend is clio's view into the database. The database could be either PostgreSQL or Cassandra.
Multiple clio servers can share access to the same database. 

`BackendInterface`, and it's derived classes, store very little state. The read methods go directly to the database,
and generally don't access any internal data structures. Nearly all of the methods are const.

The data model used by clio is called the flat map data model. The flat map data model does not store any
SHAMap inner nodes, and instead only stores the raw ledger objects contained in the leaf node. Ledger objects
are stored in the database with a compound key of `(object_id, ledger_sequence)`, where `ledger_sequence` is the
ledger in which the object was created or modified. Objects are then fetched using an inequality operation,
such as `SELECT * FROM objects WHERE object_id = id AND ledger_sequence <= seq order by ledger_sequence limit 1`, where `seq` is the ledger
in which we are trying to look up the object. When an object is deleted, we write an empty blob.

Transactions are stored in a separate table, where the key is the hash.

Ledger headers are stored in their own table.

The account_tx table maps accounts to a list of transactions that affect them.


### Backend Indexer

With the elimination of SHAMap inner nodes, iterating across a ledger becomes difficult. In order to iterate,
a keys table is maintained, which keeps a collection of all keys in a ledger. This table has one record for every
million ledgers, where each record has all of the keys in that ledger, as well as all of the keys that were deleted
between that ledger and the prior ledger written to the keys table. Most of this logic is contained in `BackendIndexer`.
