FROM ghcr.io/xrplf/clio-gcc:12.3.0 AS clio-gcc
FROM ghcr.io/xrplf/clio-tools:latest AS clio-tools

FROM ghcr.io/xrplf/clio-clang:16

ARG DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

# Using root by default is not very secure but github checkout action doesn't work with any other user
# https://github.com/actions/checkout/issues/956
# And Github Actions doc recommends using root
# https://docs.github.com/en/actions/sharing-automations/creating-actions/dockerfile-support-for-github-actions#user

# hadolint ignore=DL3002
USER root
WORKDIR /root

ARG LLVM_TOOLS_VERSION=20

# Add repositories
RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        curl \
        gnupg \
        wget \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
    && echo "deb http://apt.llvm.org/focal/ llvm-toolchain-focal-${LLVM_TOOLS_VERSION} main" >> /etc/apt/sources.list \
    && wget --progress=dot:giga -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -

# Install packages
RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        clang-tidy-${LLVM_TOOLS_VERSION} \
        clang-tools-${LLVM_TOOLS_VERSION} \
        git \
        git-lfs \
        graphviz \
        jq \
        make \
        ninja-build \
        python3 \
        python3-pip \
        zip \
    && pip3 install -q --upgrade --no-cache-dir pip \
    && pip3 install -q --no-cache-dir \
        # TODO: Remove this once we switch to newer Ubuntu base image
        # lxml 6.0.0 is not compatible with our image
        'lxml<6.0.0' \
        \
        cmake==3.31.6 \
        conan==2.17.0 \
        gcovr \
        pre-commit \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install gcc-12 and make ldconfig aware of the new libstdc++ location (for gcc)
# Note: Clang is using libc++ instead
COPY --from=clio-gcc /gcc12.deb /
RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        binutils \
        libc6-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
    && dpkg -i /gcc12.deb \
    && rm -rf /gcc12.deb \
    && ldconfig

# Rewire to use gcc-12 as default compiler
RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 \
    && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-12 100 \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-12 100 \
    && update-alternatives --install /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-12 100 \
    && update-alternatives --install /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-12 100

COPY --from=clio-tools \
    /usr/local/bin/ccache \
    /usr/local/bin/doxygen \
    /usr/local/bin/ClangBuildAnalyzer \
    /usr/local/bin/git-cliff \
    /usr/local/bin/gh \
    /usr/local/bin/

WORKDIR /root

# Setup conan
RUN conan remote add --index 0 ripple http://18.143.149.228:8081/artifactory/api/conan/dev

WORKDIR /root/.conan2
COPY conan/global.conf ./global.conf

WORKDIR /root/.conan2/profiles

COPY conan/clang.profile ./clang
COPY conan/sanitizer_template.profile ./clang.asan
COPY conan/sanitizer_template.profile ./clang.tsan
COPY conan/sanitizer_template.profile ./clang.ubsan

COPY conan/gcc.profile ./gcc
COPY conan/sanitizer_template.profile ./gcc.asan
COPY conan/sanitizer_template.profile ./gcc.tsan
COPY conan/sanitizer_template.profile ./gcc.ubsan

WORKDIR /root
