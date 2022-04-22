# Developing against `rippled` in standalone mode

If you wish you develop against a `rippled` instance running in standalone
mode there are a few quirks of both clio and rippled you need to keep in mind.
You must:

1. Advance the `rippled` ledger to at least ledger 256
2. Wait 10 minutes before first starting clio against this standalone node.
