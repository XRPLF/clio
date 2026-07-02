#!/usr/bin/env python3
import itertools
import json

LINUX_OS = ["heavy", "heavy-arm64"]
LINUX_CONTAINERS = ['{ "image": "ghcr.io/xrplf/xrpld/nix-ubuntu:sha-cb2642b" }']
LINUX_COMPILERS = ["gcc", "clang"]

MACOS_OS = ["macos-26-apple-clang-21"]
MACOS_CONTAINERS = [""]
MACOS_COMPILERS = ["apple-clang"]

BUILD_TYPES = ["Release", "Debug"]

# Values of the `SANITIZERS` environment variable read by the `sanitizers` conan
# profile. An empty string builds without any sanitizers.
SANITIZERS = ["address", "thread", "undefinedbehavior", ""]


def generate_matrix():
    configurations = []

    for os, container, compiler in itertools.chain(
        itertools.product(LINUX_OS, LINUX_CONTAINERS, LINUX_COMPILERS),
        itertools.product(MACOS_OS, MACOS_CONTAINERS, MACOS_COMPILERS),
    ):
        for sanitizers, build_type in itertools.product(SANITIZERS, BUILD_TYPES):
            configurations.append(
                {
                    "os": os,
                    "container": container,
                    "compiler": compiler,
                    "sanitizers": sanitizers,
                    "build_type": build_type,
                }
            )

    return {"include": configurations}


if __name__ == "__main__":
    print(f"matrix={json.dumps(generate_matrix())}")
