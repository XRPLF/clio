A single clio node has one or more ETL sources, specified in the config
file. clio will subscribe to the `ledgers` stream of each of the ETL
sources. This stream sends a message whenever a new ledger is validated. Upon
receiving a message on the stream, clio will then fetch the data associated
with the newly validated ledger from one of the ETL sources. The fetch is
performed via a gRPC request (`GetLedger`). This request returns the ledger
header, transactions+metadata blobs, and every ledger object
added/modified/deleted as part of this ledger. ETL then writes all of this data
to the databases, and moves on to the next ledger. ETL does not apply
transactions, but rather extracts the already computed results of those
transactions (all of the added/modified/deleted SHAMap leaf nodes of the state
tree).

If the database is entirely empty, ETL must download an entire ledger in full
(as opposed to just the diff, as described above). This download is done via the
`GetLedgerData` gRPC request. `GetLedgerData` allows clients to page through an
entire ledger over several RPC calls. ETL will page through an entire ledger,
and write each object to the database.

If the database is not empty, clio will first come up in a "soft"
read-only mode. In read-only mode, the server does not perform ETL and simply
publishes new ledgers as they are written to the database. 
If the database is not updated within a certain time period
(currently hard coded at 20 seconds), clio will begin the ETL
process and start writing to the database. The database will report an error when
trying to write a record with a key that already exists. ETL uses this error to
determine that another process is writing to the database, and subsequently
falls back to a soft read-only mode. clio can also operate in strict
read-only mode, in which case they will never write to the database.
