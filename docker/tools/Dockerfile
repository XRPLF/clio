ARG GHCR_REPO=invalid
ARG GCC_VERSION=invalid

FROM ${GHCR_REPO}/clio-gcc:${GCC_VERSION}

ARG DEBIAN_FRONTEND=noninteractive
ARG TARGETARCH

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ARG BUILD_VERSION=0

RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        python3 \
        python3-pip \
        software-properties-common \
        wget \
    && pip3 install -q --no-cache-dir \
        cmake \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp

ARG NINJA_VERSION=1.13.2

RUN wget --progress=dot:giga "https://github.com/ninja-build/ninja/archive/refs/tags/v${NINJA_VERSION}.tar.gz" \
    && tar xf "v${NINJA_VERSION}.tar.gz" \
    && cd "ninja-${NINJA_VERSION}" \
    && ./configure.py --bootstrap \
    && mv ninja /usr/local/bin/ninja \
    && rm -rf /tmp/* /var/tmp/*

ARG MOLD_VERSION=2.40.4
RUN wget --progress=dot:giga "https://github.com/rui314/mold/archive/refs/tags/v${MOLD_VERSION}.tar.gz" \
    && tar xf "v${MOLD_VERSION}.tar.gz" \
    && cd "mold-${MOLD_VERSION}" \
    && mkdir build \
    && cd build \
    && cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. \
    && ninja install \
    && rm -rf /tmp/* /var/tmp/*

ARG CCACHE_VERSION=4.12.1
RUN wget --progress=dot:giga "https://github.com/ccache/ccache/releases/download/v${CCACHE_VERSION}/ccache-${CCACHE_VERSION}.tar.gz" \
    && tar xf "ccache-${CCACHE_VERSION}.tar.gz" \
    && cd "ccache-${CCACHE_VERSION}" \
    && mkdir build \
    && cd build \
    && cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTING=False .. \
    && ninja install \
    && rm -rf /tmp/* /var/tmp/*

RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        bison \
        flex \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

ARG DOXYGEN_VERSION=1.15.0
RUN wget --progress=dot:giga "https://github.com/doxygen/doxygen/releases/download/Release_${DOXYGEN_VERSION//./_}/doxygen-${DOXYGEN_VERSION}.src.tar.gz" \
    && tar xf "doxygen-${DOXYGEN_VERSION}.src.tar.gz" \
    && cd "doxygen-${DOXYGEN_VERSION}" \
    && mkdir build \
    && cd build \
    && cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. \
    && ninja install \
    && rm -rf /tmp/* /var/tmp/*

ARG CLANG_BUILD_ANALYZER_VERSION=1.6.0
RUN wget --progress=dot:giga "https://github.com/aras-p/ClangBuildAnalyzer/archive/refs/tags/v${CLANG_BUILD_ANALYZER_VERSION}.tar.gz" \
    && tar xf "v${CLANG_BUILD_ANALYZER_VERSION}.tar.gz" \
    && cd "ClangBuildAnalyzer-${CLANG_BUILD_ANALYZER_VERSION}" \
    && mkdir build \
    && cd build \
    && cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. \
    && ninja install \
    && rm -rf /tmp/* /var/tmp/*

ARG GIT_CLIFF_VERSION=2.10.1
RUN wget --progress=dot:giga "https://github.com/orhun/git-cliff/releases/download/v${GIT_CLIFF_VERSION}/git-cliff-${GIT_CLIFF_VERSION}-x86_64-unknown-linux-musl.tar.gz" \
    && tar xf git-cliff-${GIT_CLIFF_VERSION}-x86_64-unknown-linux-musl.tar.gz \
    && mv git-cliff-${GIT_CLIFF_VERSION}/git-cliff /usr/local/bin/git-cliff \
    && rm -rf /tmp/* /var/tmp/*

ARG GH_VERSION=2.82.1
RUN wget --progress=dot:giga "https://github.com/cli/cli/releases/download/v${GH_VERSION}/gh_${GH_VERSION}_linux_${TARGETARCH}.tar.gz" \
    && tar xf gh_${GH_VERSION}_linux_${TARGETARCH}.tar.gz \
    && mv gh_${GH_VERSION}_linux_${TARGETARCH}/bin/gh /usr/local/bin/gh \
    && rm -rf /tmp/* /var/tmp/*

RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        libgmp-dev \
        libmpfr-dev \
        libncurses-dev \
        make \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

ARG GDB_VERSION=16.3
RUN wget --progress=dot:giga "https://sourceware.org/pub/gdb/releases/gdb-${GDB_VERSION}.tar.gz" \
    && tar xf "gdb-${GDB_VERSION}.tar.gz" \
    && cd "gdb-${GDB_VERSION}" \
    && ./configure --prefix=/usr/local \
    && make -j "$(nproc)" \
    && make install-gdb \
    && rm -rf /tmp/* /var/tmp/*

WORKDIR /root
