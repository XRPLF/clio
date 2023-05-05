# CLIO MIGRATOR (ONE OFF!)

This tool allows you to backfill data from
[clio](https://github.com/XRPLF/clio) due to the [specific pull request
313](https://github.com/XRPLF/clio/pull/313) in that repo.

Specifically, it is meant to migrate NFT data such that:

- The new `nf_token_uris` table is populated with all URIs for all NFTs known
- The new `issuer_nf_tokens_v2` table is populated with all NFTs known
- The old `issuer_nf_tokens` table is dropped. This table was never used prior
to the above-referenced PR, so it is very safe to drop.

## How to use

This tool should be used as follows, with regard to the above update:

1. __Compile or download the new version of `clio`__, but don't run it just yet.
2. __Stop serving requests from your existing `clio`__. If you need to achieve zero downtime, you have two options:
    - Temporarily point your traffic to someone else's `clio` that has already performed this
    migration. The XRPL Foundation should have performed this on their servers before this
    release. Ask in our Discord what server to point traffic to.
    - Create a new temporary `clio` instance running _the prior release_ and make sure
    that its config.json specifies `read_only: true`. You can safely serve data
    from this separate instance.
3. __Stop your `clio` and restart it, running the new version__. Now, your `clio` is writing new data correctly. This tool will update your
old data, while your upgraded `clio` is running and writing new ledgers.
5. __Run this tool__, using the _exact_ same config as what you are using for your
production `clio`.
6. __Once this tool terminates successfully__, you can resume serving requests
from your `clio`.


## Notes on timing

The amount of time that this migration takes depends greatly on what your data
looks like. This migration migrates data in three steps:

1. __Transaction loading__
    - Pull all successful transactions that relate to NFTs.
    The hashes of these transactions are stored in the `nf_token_transctions` table. 
    - For each of these transactions, discard any that were posted after the
    migration started
    - For each of these transactions, discard any that are not NFTokenMint
    transactions
    - For any remaning transactions, pull the associated NFT data from them and
    write them to the database.
2. __Initial ledger loading__ We need to also scan all objects in the initial
ledger, looking for any NFTokenPage objects that would not have an associated
transaction recorded.
    - Pull all objects from the initial ledger
    - For each object, if it is not an NFTokenPage, discard it.
    - Otherwise, load all NFTs stored in the NFTokenPage
3. __Drop the old (and unused) `issuer_nf_tokens` table__. This step is completely
safe, since this table is not used for anything in clio. It was meant to drive
a clio-only API called `nfts_by_issuer`, which is still in development.
However, we decided that for performance reasons its schema needed to change
to the schema we have in `issuer_nf_tokens_v2`. Since the API in question is
not yet part of clio, removing this table will not affect anything.


Step 1 is highly performance optimized. If you have a full-history clio
set-up, this migration make take only a few minutes. We tested it on a
full-history server and it completed in about 9 minutes.

However Step 2 is not well-optimized and unfortuntely cannot be. If you have a
clio server whose `start_sequence` is relatively recent (even if the
`start_sequence` indicates a ledger prior to NFTs being enabled on your
network), the migration will take longer. We tested it on a clio with a
`start_sequence` of about 1 week prior to testing and it completed in about 6
hours.

As a result, we recommend _assuming_ the worst case: that this migration will take about 8
hours.



## Compiling and running

Git-clone this project to your server. Then from the top-level directory:
```
mkdir build
cd build
cmake ..
cmake --build . -j 4
```

Once this completes, the migrator will be compiled as `clio_migrator`. Then
you should copy your existing clio config somewhere and:
```
./clio_migrator <config path>
```
