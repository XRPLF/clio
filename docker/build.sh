#!/usr/bin/env bash
set -ex

# https://github.com/docker/buildx/issues/379
ulimit -n 1024000

yum update -y
yum upgrade -y
yum install -y epel-release centos-release-scl
yum install -y \
    cmake wget \
    rpm-build gnupg \
    rh-python38 \
    devtoolset-11 \
    libstdc++-static \
    automake pkgconfig gettext libcurl-devel
yum clean all
rm -rf /var/cache/yum
