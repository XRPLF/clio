#!/bin/bash

set -ex -o pipefail

BINARY_NAME="clio_server"

ARTIFACTS_DIR="$1"
if [ -z "${ARTIFACTS_DIR}" ]; then
    echo "Usage: $0 <artifacts_directory>"
    exit 1
fi

cd "${ARTIFACTS_DIR}" || exit 1

for artifact_name in $(ls); do
    pushd "${artifact_name}" || exit 1
    zip -r "../${artifact_name}.zip" ./${BINARY_NAME}
    popd || exit 1

    rm "${artifact_name}/${BINARY_NAME}"
    rm -r "${artifact_name}"

    sha256sum "./${artifact_name}.zip" >"./${artifact_name}.zip.sha256sum"
done
