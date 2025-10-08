ARG GHCR_REPO=invalid
FROM ${GHCR_REPO}/clio-tools:latest AS clio-tools

# We're using Ubuntu 24.04 to have a more recent version of Python
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# hadolint ignore=DL3002
USER root
WORKDIR /root

# Install common tools and dependencies
RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        curl \
        git \
        software-properties-common \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install Python tools
RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        python3 \
        python3-pip \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN pip install -q --no-cache-dir --break-system-packages \
        pre-commit

COPY --from=clio-tools \
    /usr/local/bin/doxygen \
    /usr/local/bin/
