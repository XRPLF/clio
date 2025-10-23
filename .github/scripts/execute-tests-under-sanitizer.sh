#!/bin/bash

set -o pipefail

# Note: This script is intended to be run from the root of the repository.
#
# This script runs each unit-test separately and generates reports from the currently active sanitizer.
# Output is saved in ./.sanitizer-report in the root of the repository

if [[ -z "$1" ]]; then
    cat <<EOF

                                    ERROR
-----------------------------------------------------------------------------
     Path to clio_tests should be passed as first argument to the script.
-----------------------------------------------------------------------------

EOF
    exit 1
fi

TEST_BINARY=$1

if [[ ! -f "$TEST_BINARY" ]]; then
  echo "Test binary not found: $TEST_BINARY"
  exit 1
fi

TESTS=$($TEST_BINARY --gtest_list_tests | awk '/^  / {print suite $1} !/^  / {suite=$1}')

OUTPUT_DIR="./.sanitizer-report"
mkdir -p "$OUTPUT_DIR"

export TSAN_OPTIONS="die_after_fork=0"
export MallocNanoZone='0' # for MacOSX

for TEST in $TESTS; do
  OUTPUT_FILE="$OUTPUT_DIR/${TEST//\//_}.log"
  $TEST_BINARY --gtest_filter="$TEST" > "$OUTPUT_FILE" 2>&1

  if [ $? -ne 0 ]; then
    echo "'$TEST' failed a sanitizer check."
  else
    rm "$OUTPUT_FILE"
  fi
done
