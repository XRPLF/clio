#!/usr/bin/env bash

set -ex
GIT_VERSION="2.37.1"
curl -OJL https://github.com/git/git/archive/refs/tags/v${GIT_VERSION}.tar.gz
tar zxvf git-${GIT_VERSION}.tar.gz
cd git-${GIT_VERSION}

yum install -y centos-release-scl epel-release
yum update -y
yum install -y devtoolset-11 autoconf gnu-getopt gettext zlib-devel  libcurl-devel

source /opt/rh/devtoolset-11/enable
make configure
./configure
make git -j$(nproc)
make install git
git  --version | cut -d ' ' -f3
