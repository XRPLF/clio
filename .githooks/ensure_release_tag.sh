#!/bin/bash

# Pushing a release branch requires an annotated tag at the released commit
branch=$(git rev-parse --abbrev-ref HEAD)

if [[ $branch =~ master|release/* ]]; then
  IFS=/ read -r branch rel_ver <<< ${branch}
  tag=$(git describe --tags --abbrev=0)
  if [[ "${rel_ver}" != "${tag}" ]]; then
    echo "master and release/${rel_ver} branches must have annotated tag ${rel_ver}"
    echo "git tag -am\"${rel_ver}\" ${rel_ver}"
    exit 1
  fi
fi
