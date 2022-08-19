#!/usr/bin/env bash
set -exu

#yum install wget lz4 lz4-devel git llvm13-static.x86_64 llvm13-devel.x86_64 devtoolset-11-binutils zlib-static
# it's either those or link=static that halves the failures. probably link=static
BOOST_VERSION=$1
BOOST_VERSION_=$(echo ${BOOST_VERSION} | tr . _)
echo "BOOST_VERSION: ${BOOST_VERSION}"
echo "BOOST_VERSION_: ${BOOST_VERSION_}"
curl -OJLs "https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_}.tar.gz"
tar zxf "boost_${BOOST_VERSION_}.tar.gz"
cd boost_${BOOST_VERSION_} && ./bootstrap.sh && ./b2 --without-python link=static -j$(nproc)
mkdir -p /boost && mv boost /boost && mv stage /boost
