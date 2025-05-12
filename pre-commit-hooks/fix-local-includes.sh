#!/bin/bash

# Note: This script is intended to be run from the root of the repository.
#
# This script checks will fix local includes in the C++ code.

file_path="$1"

echo "+ Fixing includes in $file_path..."

GNU_SED=$(sed --version 2>&1 | grep -q 'GNU' && echo true || echo false)

if [[ "$GNU_SED" == "false" ]]; then # macOS sed
    # make all includes to be <...> style
    sed -i '' -E 's|#include "(.*)"|#include <\1>|g' "$file_path"

    # make local includes to be "..." style
    main_src_dirs=$(find ./src -maxdepth 1 -type d -exec basename {} \; | tr '\n' '|' | sed 's/|$//' | sed 's/|/\\|/g')
    sed -i '' -E "s|#include <(($main_src_dirs)/.*)>|#include \"\1\"|g" "$file_path"
else
    # make all includes to be <...> style
    sed -i -E 's|#include "(.*)"|#include <\1>|g' "$file_path"

    # make local includes to be "..." style
    main_src_dirs=$(find ./src -maxdepth 1 -type d  -exec basename {} \; | paste -sd '|' | sed 's/|/\\|/g')
    sed -i -E "s|#include <(($main_src_dirs)/.*)>|#include \"\1\"|g" "$file_path"
fi
