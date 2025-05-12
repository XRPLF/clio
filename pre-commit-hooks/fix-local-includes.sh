#!/bin/bash

# Note: This script is intended to be run from the root of the repository.
#
# This script will fix local includes in the C++ code for a given file.
# Usage: ./pre-commit-hooks/fix-local-includes.sh <file1> <file2> ...

files="$@"
echo "+ Fixing includes in $files..."

GNU_SED=$(sed --version 2>&1 | grep -q 'GNU' && echo true || echo false)

if [[ "$GNU_SED" == "false" ]]; then # macOS sed
    main_src_dirs=$(find ./src -maxdepth 1 -type d -exec basename {} \; | tr '\n' '|' | sed 's/|$//' | sed 's/|/\\|/g')
else
    main_src_dirs=$(find ./src -maxdepth 1 -type d  -exec basename {} \; | paste -sd '|' | sed 's/|/\\|/g')
fi

fix_includes() {
    file_path="$1"

    if [[ "$GNU_SED" == "false" ]]; then # macOS sed
        # make all includes to be <...> style
        sed -i '' -E 's|#include "(.*)"|#include <\1>|g' "$file_path"

        # make local includes to be "..." style
        sed -i '' -E "s|#include <(($main_src_dirs)/.*)>|#include \"\1\"|g" "$file_path"
    else
        # make all includes to be <...> style
        sed -i -E 's|#include "(.*)"|#include <\1>|g' "$file_path"

        # make local includes to be "..." style
        sed -i -E "s|#include <(($main_src_dirs)/.*)>|#include \"\1\"|g" "$file_path"
    fi
}

for file in $files; do
    fix_includes "$file"
done
