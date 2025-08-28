#!/bin/bash

set -ex

CURRENT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_DIR="$(cd "$CURRENT_DIR/../../../" && pwd)"

CONAN_DIR="${CONAN_HOME:-$HOME/.conan2}"
PROFILES_DIR="$CONAN_DIR/profiles"

# When developers' compilers are updated, these profiles might be different
if [[ -z "$CI" ]]; then
    APPLE_CLANG_PROFILE="$CURRENT_DIR/apple-clang-17.profile"
else
    APPLE_CLANG_PROFILE="$CURRENT_DIR/apple-clang-17.profile"
fi

GCC_PROFILE="$REPO_DIR/docker/ci/conan/gcc.profile"
CLANG_PROFILE="$REPO_DIR/docker/ci/conan/clang.profile"

SANITIZER_TEMPLATE_FILE="$REPO_DIR/docker/ci/conan/sanitizer_template.profile"

rm -rf "$CONAN_DIR"

conan remote add --index 0 ripple https://conan.ripplex.io

cp "$REPO_DIR/docker/ci/conan/global.conf" "$CONAN_DIR/global.conf"

create_profile_with_sanitizers() {
    profile_name="$1"
    profile_source="$2"

    cp "$profile_source" "$PROFILES_DIR/$profile_name"
    cp "$SANITIZER_TEMPLATE_FILE" "$PROFILES_DIR/$profile_name.asan"
    cp "$SANITIZER_TEMPLATE_FILE" "$PROFILES_DIR/$profile_name.tsan"
    cp "$SANITIZER_TEMPLATE_FILE" "$PROFILES_DIR/$profile_name.ubsan"
}

mkdir -p "$PROFILES_DIR"

if [[ "$(uname)" == "Darwin" ]]; then
    create_profile_with_sanitizers "apple-clang" "$APPLE_CLANG_PROFILE"
    echo "include(apple-clang)" > "$PROFILES_DIR/default"
else
    create_profile_with_sanitizers "clang" "$CLANG_PROFILE"
    create_profile_with_sanitizers "gcc" "$GCC_PROFILE"
    echo "include(gcc)" > "$PROFILES_DIR/default"
fi
