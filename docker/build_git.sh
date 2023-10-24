#!/usr/bin/env bash

set -ex
GIT_VERSION=${GIT_VERSION:-"2.42.0"}
curl -OJL https://github.com/git/git/archive/refs/tags/v${GIT_VERSION}.tar.gz
tar zxvf git-${GIT_VERSION}.tar.gz && rm git-${GIT_VERSION}.tar.gz

cd git-${GIT_VERSION}

source /opt/rh/devtoolset-11/enable

make configure
./configure
make git -j$(nproc)
make install git
cd ..
rm -rf git-${GIT_VERSION}
git  --version | cut -d ' ' -f3
