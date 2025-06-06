# TODO: change this when we are able to push gcc image to ghcr.io
FROM rippleci/clio_gcc:12.3.0 AS clio-gcc
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

ARG LLVM_TOOLS_VERSION=19

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
        bison \
        clang-tidy-${LLVM_TOOLS_VERSION} \
        clang-tools-${LLVM_TOOLS_VERSION} \
        flex \
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
        cmake==3.31.6 \
        conan==1.62 \
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
RUN conan remote add --insert 0 conan-non-prod http://18.143.149.228:8081/artifactory/api/conan/conan-non-prod

# Note: intentionally leaving cppstd=20
RUN conan profile new gcc --detect \
    && conan profile update settings.compiler=gcc gcc \
    && conan profile update settings.compiler.version=12 gcc \
    && conan profile update settings.compiler.cppstd=20 gcc \
    && conan profile update settings.compiler.libcxx=libstdc++11 gcc \
    && conan profile update env.CC=/usr/bin/gcc-12 gcc \
    && conan profile update env.CXX=/usr/bin/g++-12 gcc \
    && conan profile update "conf.tools.build:compiler_executables={\"c\": \"/usr/bin/gcc-12\", \"cpp\": \"/usr/bin/g++-12\"}" gcc

RUN conan profile new clang --detect \
    && conan profile update settings.compiler=clang clang \
    && conan profile update settings.compiler.version=16 clang \
    && conan profile update settings.compiler.cppstd=20 clang \
    && conan profile update settings.compiler.libcxx=libc++ clang \
    && conan profile update env.CC=/usr/bin/clang-16 clang \
    && conan profile update env.CXX=/usr/bin/clang++-16 clang \
    && conan profile update env.CXXFLAGS="-DBOOST_ASIO_DISABLE_CONCEPTS" clang \
    && conan profile update "conf.tools.build:compiler_executables={\"c\": \"/usr/bin/clang-16\", \"cpp\": \"/usr/bin/clang++-16\"}" clang

RUN echo "include(gcc)" >> .conan/profiles/default

COPY conan/gcc.asan /root/.conan/profiles
COPY conan/gcc.tsan /root/.conan/profiles
COPY conan/gcc.ubsan /root/.conan/profiles
COPY conan/clang.asan /root/.conan/profiles
COPY conan/clang.tsan /root/.conan/profiles
COPY conan/clang.ubsan /root/.conan/profiles
