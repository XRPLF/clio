#!/usr/bin/env bash
set -ex

REF=$(git rev-parse --short HEAD)
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

IMAGE_NAME=${1:-clio\:$REF}

if [[ $(basename $PWD) == "docker" ]]; then
    cd ..
fi

echo "CLIO_GIT_REF=$REF" >> $SCRIPT_DIR/.env
# # build the clio builder image
#docker build . --target builder --tag clio-builder:$REF  --file docker/Dockerfile

# # build clio
#docker build . --target build_clio --tag clio-built:$REF --file docker/Dockerfile

# # build the clio server image
docker build .  --target clio --tag "${IMAGE_NAME}" --file docker/Dockerfile
