#!/usr/bin/env python3
import itertools
import json

LINUX_OS = ["heavy", "heavy-arm64"]
LINUX_CONTAINERS = ['{ "image": "ghcr.io/xrplf/clio-ci:latest" }']
LINUX_COMPILERS = ["gcc", "clang"]

MACOS_OS = ["macos15"]
MACOS_CONTAINERS = [""]
MACOS_COMPILERS = ["apple-clang"]

BUILD_TYPES = ["Release", "Debug"]
SANITIZER_EXT = [".asan", ".tsan", ".ubsan", ""]


def generate_matrix():
    configurations = []

    for os, container, compiler in itertools.chain(
        itertools.product(LINUX_OS, LINUX_CONTAINERS, LINUX_COMPILERS),
        itertools.product(MACOS_OS, MACOS_CONTAINERS, MACOS_COMPILERS),
    ):
        for sanitizer_ext, build_type in itertools.product(SANITIZER_EXT, BUILD_TYPES):
            # libbacktrace doesn't build on arm64 with gcc.tsan
            if os == "heavy-arm64" and compiler == "gcc" and sanitizer_ext == ".tsan":
                continue
            configurations.append(
                {
                    "os": os,
                    "container": container,
                    "compiler": compiler,
                    "sanitizer_ext": sanitizer_ext,
                    "build_type": build_type,
                }
            )

    return {"include": configurations}


if __name__ == "__main__":
    print(f"matrix={json.dumps(generate_matrix())}")
