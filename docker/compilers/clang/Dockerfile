FROM ubuntu:20.04

ARG DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-c"]

# hadolint ignore=DL3002
USER root
WORKDIR /root

ARG CLANG_VERSION=16

RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        wget \
        software-properties-common \
        gnupg \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN wget --progress=dot:giga https://apt.llvm.org/llvm.sh \
    && chmod +x llvm.sh \
    && ./llvm.sh ${CLANG_VERSION} \
    && rm -rf llvm.sh \
    && apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        libc++-${CLANG_VERSION}-dev \
        libc++abi-${CLANG_VERSION}-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
