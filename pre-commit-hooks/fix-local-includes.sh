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
    main_src_dirs=$(find ./src -maxdepth 1 -type d -exec basename {} \; | paste -sd '|' | sed 's/|/\\|/g')
fi

fix_includes() {
    file_path="$1"

    file_path_all_global="${file_path}.tmp.global"
    file_path_fixed="${file_path}.tmp.fixed"

    # Make all includes to be <...> style
    sed -E 's|#include "(.*)"|#include <\1>|g' "$file_path" > "$file_path_all_global"

    # Make local includes to be "..." style
    sed -E "s|#include <(($main_src_dirs)/.*)>|#include \"\1\"|g" "$file_path_all_global" > "$file_path_fixed"
    rm "$file_path_all_global"

    # Check if the temporary file is different from the original file
    if ! cmp -s "$file_path" "$file_path_fixed"; then
        # Replace the original file with the temporary file
        mv "$file_path_fixed" "$file_path"
    else
        # Remove the temporary file if it's the same as the original
        rm "$file_path_fixed"
    fi
}

for file in $files; do
    fix_includes "$file"
done
