ARG GCC_VERSION=invalid

FROM ghcr.io/xrplf/clio-gcc:${GCC_VERSION}

ARG DEBIAN_FRONTEND=noninteractive
ARG TARGETARCH

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

ARG BUILD_VERSION=1

RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        bison \
        flex \
        ninja-build \
        python3 \
        python3-pip \
        software-properties-common \
        wget \
    && pip3 install -q --no-cache-dir \
        cmake==3.31.6 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp

ARG MOLD_VERSION=2.40.1
RUN wget --progress=dot:giga "https://github.com/rui314/mold/archive/refs/tags/v${MOLD_VERSION}.tar.gz" \
    && tar xf "v${MOLD_VERSION}.tar.gz" \
    && cd "mold-${MOLD_VERSION}" \
    && mkdir build \
    && cd build \
    && cmake -GNinja -DCMAKE_BUILD_TYPE=Release .. \
    && ninja install \
    && rm -rf /tmp/* /var/tmp/*

ARG CCACHE_VERSION=4.11.3
RUN wget --progress=dot:giga "https://github.com/ccache/ccache/releases/download/v${CCACHE_VERSION}/ccache-${CCACHE_VERSION}.tar.gz" \
    && tar xf "ccache-${CCACHE_VERSION}.tar.gz" \
    && cd "ccache-${CCACHE_VERSION}" \
    && mkdir build \
    && cd build \
    && cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTING=False .. \
    && ninja install \
    && rm -rf /tmp/* /var/tmp/*

ARG DOXYGEN_VERSION=1.12.0
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

ARG GIT_CLIFF_VERSION=2.9.1
RUN wget --progress=dot:giga "https://github.com/orhun/git-cliff/releases/download/v${GIT_CLIFF_VERSION}/git-cliff-${GIT_CLIFF_VERSION}-x86_64-unknown-linux-musl.tar.gz" \
    && tar xf git-cliff-${GIT_CLIFF_VERSION}-x86_64-unknown-linux-musl.tar.gz \
    && mv git-cliff-${GIT_CLIFF_VERSION}/git-cliff /usr/local/bin/git-cliff \
    && rm -rf /tmp/* /var/tmp/*

ARG GH_VERSION=2.74.0
RUN wget --progress=dot:giga "https://github.com/cli/cli/releases/download/v${GH_VERSION}/gh_${GH_VERSION}_linux_${TARGETARCH}.tar.gz" \
    && tar xf gh_${GH_VERSION}_linux_${TARGETARCH}.tar.gz \
    && mv gh_${GH_VERSION}_linux_${TARGETARCH}/bin/gh /usr/local/bin/gh \
    && rm -rf /tmp/* /var/tmp/*

WORKDIR /root
