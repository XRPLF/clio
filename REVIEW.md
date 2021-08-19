# How to review clio
Clio is a massive project, and thus I don't expect the code to be reviewed the
way a normal PR would. So I put this guide together to help reviewers look at
the relevant pieces of code without getting lost in the weeds.

One thing reviewers should keep in mind is that most of clio is designed to be
lightweight and simple. We try not to introduce any uneccessary complexity and
keep the code as simple and straightforward as possible. Sometimes complexity is
unavoidable, but simplicity is the goal.

## Order of review
The code is organized into 4 main components, each with their own folder. The
code in each folder is as self contained as possible. A good way to approach
the review would be to review one folder at a time.

### backend
The code in the backend folder is the heart of the project, and reviewers should
start here. This is the most complex part of the code, as well as the most
performance sensitive. clio does not keep any data in memory, so performance
generally depends on the data model and the way we talk to the database.

Reviewers should start with the README in this folder to get a high level idea
of the data model and to review the data model itself. Then, reviewers should
dive into the implementation. A good way to approach the implementation would
be to go through the functions in BackendInterface, and review each of their
implementations in PostgresBackend and CassandraBackend. Reviewers can also
look at how these functions are being used. This will take you into other parts
of the code, which you can review as well.

### server
The code in the server folder implements the web server for handling RPC requests.
This code was mostly copied and pasted from boost beast example code, so I would
really appreciate review here.

### rpc
The rpc folder contains all of the handlers and any helper functions they need.
This code is not too complicated, so reviewers don't need to dwell long here.

### etl
The etl folder contains all of the code for extracting data from rippled. This
code is complex and important, but most of this code was just copied from rippled
reporting mode, and thus has already been reviewed and is being used in prod.

## Design decisions that should be reviewed

### Data model
Reviewers should review the general data model. The data model itself is described
at a high level in the README in the backend folder. The table schemas and queries
for Cassandra are defined in the `open()` function of `CassandraBackend`. The table
schemas for Postgres are defined in Pg.cpp. 

Particular attention should be paid to the keys table, and the problem that solves
(successor/upper bound). I originally was going to have a special table for book_offers,
but then I decided that we could use the keys table itself for that and save space.
This makes book_offers somewhat slow compared to rippled, though still very usable.

### Large rows
I did some tricks with Cassandra to deal with very large rows in the keys and account_tx
tables. For each of these, the partition key (the first component of the primary
key) is a compound key. This is meant to break large rows into smaller rows. This
is done to avoid hotspots. Data is sharded in Cassandra, and if some rows get very
large, some nodes can have a lot more data than others.

For account_tx, this has performance implications when iterating very far back
in time. Refer to the `fetchAccountTransactions()` function in `CassandraBackend`.

It is unclear if this needs to be done for other tables.

### Threading
I used asio for multithreading. There are a lot of different io_contexts lying
around the code. This needs to be cleaned up a bit. Most of these are really
just ways to submit an async job to a single thread. I don't think it makes
sense to have one io_context for the whole application, but some of the threading
is a bit opaque and could be cleaned up.

### Boost Json
I used boost json for serializing data to json.

### No cache
As of now, there is no cache. I am not sure if a cache is even worth it. A
transaction cache would not be hard, but a cache for ledger data will be hard.
While a cache would improve performance, it would increase memory usage. clio
is designed to be lightweight.

## Things I'm less than happy about

#### BackendIndexer
This is a particularly hairy piece of code that handles writing to the keys table.
I am not too happy with this code. Parts of it need to execute in real time as
part of ETL, and other parts are allowed to run in the background. There is also
code that detects if a previous background job failed to complete before the
server shutdown, and thus tries to rerun that job. The code feels tacked on, and
I would like it to be more cleanly integrated with the rest of the code.

#### Shifting
There is some bit shifting going on with the keys table and the account_tx table.
The keys table is written to every 2^20 ledgers. Maybe it would be better to just
write every 1 million ledgers. 

#### performance of book_offers
book_offers is a bit slow. It could be sped up in a variety of ways. One is to
keep a separate book_offers table. However, this is not straightforward and will
use more space. Another is to keep a cache of book_offers for the most recent ledger
(or few ledgers). I am not sure if this is worth it

#### account_tx in Cassandra
After the fix to deal with large rows, account_tx can be slow at times when using
Cassandra. Specifically, if there are large gaps in time where the account was
not affected by any transactions, the code will be reading empty records. I would
like to sidestep this issue if possible.

#### Implementation of fetchLedgerPage
`fetchLedgerPage()` is rather complex. Part of this seems unavoidable, since this
code is dealing with the keys table.
