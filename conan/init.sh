#!/bin/bash

set -ex

CURRENT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROFILES_SRC_DIR="$CURRENT_DIR/profiles"

CONAN_DIR="${CONAN_HOME:-$HOME/.conan2}"
PROFILES_DIR="$CONAN_DIR/profiles"

rm -rf "$CONAN_DIR"

conan remote add --index 0 --force xrplf https://conan.xrplf.org/repository/conan/

cp "$CURRENT_DIR/global.conf" "$CONAN_DIR/global.conf"

mkdir -p "$PROFILES_DIR"

# The compiler is selected via the `CC`/`CXX` environment variables (see
# `.github/actions/set-compiler-env`) and the sanitizers via the `SANITIZERS`
# environment variable. Builds always use the `ci` profile, which includes
# `sanitizers` and `default`.
cp "$PROFILES_SRC_DIR/default" "$PROFILES_DIR/default"
cp "$PROFILES_SRC_DIR/ci" "$PROFILES_DIR/ci"
cp "$PROFILES_SRC_DIR/sanitizers" "$PROFILES_DIR/sanitizers"
