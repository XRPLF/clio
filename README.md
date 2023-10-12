# CLIO MIGRATOR (ONE OFF!)

This tool allows you to backfill data from
[clio](https://github.com/XRPLF/clio) due to the [specific pull request
313](https://github.com/XRPLF/clio/pull/313) in that repo.

Specifically, it is meant to migrate NFT data such that:

- The new `nf_token_uris` table is populated with all URIs for all NFTs known
- The new `issuer_nf_tokens_v2` table is populated with all NFTs known
- The old `issuer_nf_tokens` table is dropped. This table was never used prior
to the above-referenced PR, so it is very safe to drop.

## Overall Migration Steps
This tool should be used as follows, with regard to the above update:

0. __Copy your current clio configuration file to this repo__.
1. __Compile or download the new version of `clio`__, but don't run it just yet.
2. __Stop serving requests from your existing `clio`__. If you need to achieve zero downtime, you have two options:
    - Temporarily point your traffic to someone else's `clio` that has already performed this
    migration. The XRPL Foundation should have performed this on their servers before this
    release. Ask in our Discord what server to point traffic to.
    - Create a new temporary `clio` instance running _the prior release_ and make sure
    that its config.json specifies `read_only: true`. You can safely serve data
    from this separate instance.
3. __Stop your `clio` and restart it, running the new version__. You may make any changes necessary to the config,
but do not change the old configuration you copied in Step 0.
4. __Now, your `clio` is writing new data correctly__. This tool will update your
old data, while your upgraded `clio` is running and writing new ledgers.
5. __Run this tool__, using the configuration file you copied in Step 0.
   Details on how to run the tool are below in the "usage" section.
6. __Once this tool terminates successfully__, you can resume serving requests
from your `clio`.
7. __Optionally__ you can now use the included `clio_verifier` executable to
   ensure that URIs were migrated correctly. This executable exit with a 0
   status code if everything is OK. It will take a long time to run, and is
   meant to be run while you are already fully running on the new version.
   Details on how to run this tool are below in the "usage" section.

## Usage
### Compiling
> **Note:** The migrator uses **Boost 1.75.0**. You have to point the `BOOST_ROOT` env variable to its root directory.

Git-clone this project to your server. Then from the top-level directory:
```bash
mkdir build
cd build
BOOST_ROOT=/path/to/boost_1_75_0 cmake ..
cmake --build . -j 4
```

### Running the migration
Once this completes, the migrator will be compiled as `clio_migrator`. Then
you should use the old config file you copied in Step 0 above to run it like
so:
```bash
./clio_migrator <config path>
```

### OPTIONAL: running the verifier
After the migration completes, it is optional to perform a database verification to ensure the URIs are migrated correctly.
Again, use the old config file you copied in Step 0 above.
```bash
./clio_verifier <config path>
```

## Technical details and notes on timing
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
2. __Initial ledger loading__. We need to also scan all objects in the initial
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
set-up, this migration may take only a few minutes. We tested it on a
full-history server and it completed in about 9 minutes.

However Step 2 is not well-optimized and unfortunately cannot be. If you have a
clio server whose `start_sequence` is relatively recent (even if the
`start_sequence` indicates a ledger prior to NFTs being enabled on your
network), the migration will take longer. We tested it on a clio with a
`start_sequence` of about 1 week prior to testing and it completed in about 6
hours.

As a result, we recommend _assuming_ the worst case: that this migration will take about 8
hours.
