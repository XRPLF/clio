FROM ubuntu:20.04 AS build

ARG DEBIAN_FRONTEND=noninteractive
ARG TARGETARCH
ARG UBUNTU_VERSION=20.04
ARG GCC_VERSION=12.3.0
ARG BUILD_VERSION=2

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

RUN wget --progress=dot:giga https://gcc.gnu.org/pub/gcc/releases/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz \
    && tar xf gcc-$GCC_VERSION.tar.gz \
    && cd /gcc-$GCC_VERSION && ./contrib/download_prerequisites

RUN mkdir /${TARGETARCH}-gcc-12
WORKDIR /${TARGETARCH}-gcc-12
RUN /gcc-$GCC_VERSION/configure \
        --with-pkgversion="clio-build-$BUILD_VERSION https://github.com/XRPLF/clio" \
        --enable-languages=c,c++ \
        --prefix=/usr \
        --with-gcc-major-version-only \
        --program-suffix=-12 \
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
        --enable-checking=release \
    && make -j "$(nproc)" \
    && make install-strip DESTDIR=/gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION \
    && mkdir -p /gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION/usr/share/gdb/auto-load/usr/lib64 \
    && mv /gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION/usr/lib64/libstdc++.so.6.0.30-gdb.py /gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION/usr/share/gdb/auto-load/usr/lib64/libstdc++.so.6.0.30-gdb.py

# Generate deb
WORKDIR /
COPY control.m4 /
COPY ld.so.conf /gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION/etc/ld.so.conf.d/1-gcc-12.conf

RUN mkdir /gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION/DEBIAN \
    && m4 -P -DUBUNTU_VERSION=$UBUNTU_VERSION -DVERSION=$GCC_VERSION-$BUILD_VERSION -DTARGETARCH=$TARGETARCH control.m4 > /gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION/DEBIAN/control \
    && dpkg-deb --build --root-owner-group /gcc-$GCC_VERSION-$BUILD_VERSION-ubuntu-$UBUNTU_VERSION /gcc12.deb

# Create final image
FROM ubuntu:20.04
COPY --from=build /gcc12.deb /

# Make gcc-12 available but also leave gcc12.deb for others to copy if needed
RUN apt-get update \
    && apt-get install -y --no-install-recommends --no-install-suggests \
        binutils \
        libc6-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
    && dpkg -i /gcc12.deb

RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 \
    && update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++-12 100 \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/cc cc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-12 100 \
    && update-alternatives --install /usr/bin/gcov-dump gcov-dump /usr/bin/gcov-dump-12 100 \
    && update-alternatives --install /usr/bin/gcov-tool gcov-tool /usr/bin/gcov-tool-12 100
