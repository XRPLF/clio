#!/bin/bash
rg -l '#include <.*"' | xargs sed -i -E 's|#include "(.*)>|#include <\1>|g'
for d in $(find ./src -maxdepth 1 -type d -execdir basename {} \;); do
    rg -l "#include <$d/.*>" | xargs sed -i -E "s|#include <($d/.*)>|#include \"\1\"|g"
done

