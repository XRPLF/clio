#!/usr/bin/env bash
set -ex

CMAKE_VERSION=${CMAKE_VERSION:-"3.27.7"}

IFS='.' read -ra CMAKE_VERSION_ARRAY  <<< $CMAKE_VERSION
read -r CMAKE_MAJ_VERSION CMAKE_MIN_VERSION CMAKE_PATCH_VERSIONc <<< ${CMAKE_VERSION_ARRAY[@]}
CMAKE_SCRIPT="cmake-${CMAKE_VERSION}-linux-x86_64.sh"

curl -OJL "https://cmake.org/files/v${CMAKE_MAJ_VERSION}.${CMAKE_MIN_VERSION}/${CMAKE_SCRIPT}"
chmod +x "${CMAKE_SCRIPT}"

./"${CMAKE_SCRIPT}" --skip-license --prefix=/usr/local
rm "${CMAKE_SCRIPT}"
