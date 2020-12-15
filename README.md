# CLIO MIGRATOR (ONE OFF!)

This tool is a (really) hacky way of migrating some data from
[clio](https://github.com/XRPLF/clio) due to the [specific pull request
313](https://github.com/XRPLF/clio/pull/313) in that repo.

Specifically, it is meant to migrate NFT data such that:

* The new `nf_token_uris` table is populated with all URIs for all NFTs known
* The new `issuer_nf_tokens_v2` table is populated with all NFTs known
* The old `issuer_nf_tokens` table is dropped. This table was never used prior
    to the above-referenced PR, so it is very safe to drop.

This tool should be used as follows, with regard to the above update:

1) Stop serving requests from your clio
2) Stop your clio and upgrade it to the version after the after PR
3) Start your clio
4) Now, your clio is writing new data correctly. This tool will update your
old data, while your new clio is running.
5) Run this tool, using the _exact_ same config as what you are using for your
production clio.
6) Once this tool terminates successfully, you can resume serving requests
from your clio.


## Compiling

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

This migration will take a few hours to complete.
