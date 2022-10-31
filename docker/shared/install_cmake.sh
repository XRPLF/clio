#!/bin/bash

set -eo pipefail

CMAKE_VERSION=${1:-"3.16.3"}
cd /tmp
URL="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz"
curl -OJLs $URL
tar xzvf cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz
mv cmake-${CMAKE_VERSION}-Linux-x86_64 /opt/
ln -s /opt/cmake-${CMAKE_VERSION}-Linux-x86_64/bin/cmake /usr/local/bin/cmake
