#!/usr/bin/env bash

set -ex

TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

echo "Using temporary CONAN_HOME: $TEMP_DIR"

# We use a temporary Conan home to avoid polluting the user's existing Conan
# configuration and to not use local cache (which leads to non-reproducible lockfiles).
export CONAN_HOME="$TEMP_DIR"

# Ensure that the xrplf remote is the first to be consulted, so any recipes we
# patched are used. We also add it there to not created huge diff when the
# official Conan Center Index is updated.
conan remote add --force --index 0 xrplf https://conan.xrplf.org/repository/conan/

# Delete any existing lockfile.
rm -f conan.lock

# Create a new lockfile that is compatible with macOS.
# It should also work on Linux.
conan lock create . \
    --profile:all=./conan/lockfile/apple-clang-21.profile
