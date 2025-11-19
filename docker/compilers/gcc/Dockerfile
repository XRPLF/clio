ARG UBUNTU_VERSION=20.04

ARG GCC_MAJOR_VERSION=invalid

FROM ubuntu:$UBUNTU_VERSION AS build

ARG UBUNTU_VERSION

ARG GCC_MAJOR_VERSION

ARG BUILD_VERSION=0

ARG DEBIAN_FRONTEND=noninteractive
ARG TARGETARCH

RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        build-essential \
        file \
        flex \
        libz-dev \
        libzstd-dev \
        software-properties-common \
        wget \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

ARG GCC_VERSION

WORKDIR /
RUN wget --progress=dot:giga https://gcc.gnu.org/pub/gcc/releases/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz \
    && tar xf gcc-$GCC_VERSION.tar.gz

WORKDIR /gcc-$GCC_VERSION
RUN ./contrib/download_prerequisites

# hadolint ignore=DL3059
RUN mkdir /gcc-build
WORKDIR /gcc-build
RUN /gcc-$GCC_VERSION/configure \
        --with-pkgversion="clio-build-$BUILD_VERSION https://github.com/XRPLF/clio" \
        --enable-languages=c,c++ \
        --prefix=/usr \
        --with-gcc-major-version-only \
        --program-suffix=-${GCC_MAJOR_VERSION} \
        --enable-shared \
        --enable-linker-build-id \
        --libexecdir=/usr/lib \
        --without-included-gettext \
        --enable-threads=posix \
        --libdir=/usr/lib \
        --disable-nls \
        --enable-clocale=gnu \
        --enable-libstdcxx-backtrace=yes \
        --enable-libstdcxx-debug \
        --enable-libstdcxx-time=yes \
        --with-default-libstdcxx-abi=new \
        --enable-gnu-unique-object \
        --disable-vtable-verify \
        --enable-plugin \
        --enable-default-pie \
        --with-system-zlib \
        --enable-libphobos-checking=release \
        --with-target-system-zlib=auto \
        --disable-werror \
        --enable-cet \
        --disable-multilib \
        --without-cuda-driver \
        --enable-checking=release

RUN make -j "$(nproc)"
RUN make install-strip DESTDIR=/gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION

RUN export GDB_AUTOLOAD_DIR="/gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION/usr/share/gdb/auto-load/usr/lib64" \
    && mkdir -p "$GDB_AUTOLOAD_DIR" \
    && mv \
        /gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION/usr/lib64/libstdc++.so.*-gdb.py \
        $GDB_AUTOLOAD_DIR/

# Generate deb
WORKDIR /
COPY control.m4 /
COPY ld.so.conf /gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION/etc/ld.so.conf.d/1-gcc-${GCC_MAJOR_VERSION}.conf

RUN mkdir /gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION/DEBIAN \
    && m4 \
        -P \
        -DUBUNTU_VERSION=$UBUNTU_VERSION \
        -DVERSION=$GCC_VERSION-$BUILD_VERSION \
        -DTARGETARCH=$TARGETARCH \
        control.m4 > /gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION/DEBIAN/control \
    && dpkg-deb \
        --build \
        --root-owner-group \
        /gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION \
        /gcc${GCC_MAJOR_VERSION}.deb

# Create final image
FROM ubuntu:$UBUNTU_VERSION

ARG GCC_MAJOR_VERSION

COPY --from=build /gcc${GCC_MAJOR_VERSION}.deb /

# Install gcc-${GCC_MAJOR_VERSION}, but also leave gcc${GCC_MAJOR_VERSION}.deb for others to copy if needed
RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        binutils \
        libc6-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
    && dpkg -i /gcc${GCC_MAJOR_VERSION}.deb

RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${GCC_MAJOR_VERSION} 100 \
    && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-${GCC_MAJOR_VERSION} 100 \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_MAJOR_VERSION} 100 \
    && update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-${GCC_MAJOR_VERSION} 100 \
    && update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-${GCC_MAJOR_VERSION} 100 \
    && update-alternatives --install /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-${GCC_MAJOR_VERSION} 100 \
    && update-alternatives --install /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-${GCC_MAJOR_VERSION} 100
