#!/bin/bash

# Note: This script is intended to be run from the root of the repository.
#
# This script checks will fix local includes in the C++ code.

# paths to fix include statements
sources="src tests"

echo "+ Fixing local includes..."

function grep_code {
    grep -l "${1}" ${sources} -r --include \*.hpp --include \*.cpp
}

GNU_SED=$(sed --version 2>&1 | grep -q 'GNU' && echo true || echo false)

if [[ "$GNU_SED" == "false" ]]; then # macOS sed
    # make all includes to be <...> style
    grep_code '#include ".*"' | xargs sed -i '' -E 's|#include "(.*)"|#include <\1>|g'

    # make local includes to be "..." style
    main_src_dirs=$(find ./src -maxdepth 1 -type d -exec basename {} \; | tr '\n' '|' | sed 's/|$//' | sed 's/|/\\|/g')
    grep_code "#include <\($main_src_dirs\)/.*>" | xargs sed -i '' -E "s|#include <(($main_src_dirs)/.*)>|#include \"\1\"|g"
else
    # make all includes to be <...> style
    grep_code '#include ".*"' | xargs sed -i -E 's|#include "(.*)"|#include <\1>|g'

    # make local includes to be "..." style
    main_src_dirs=$(find ./src -maxdepth 1 -type d  -exec basename {} \; | paste -sd '|' | sed 's/|/\\|/g')
    grep_code "#include <\($main_src_dirs\)/.*>" | xargs sed -i -E "s|#include <(($main_src_dirs)/.*)>|#include \"\1\"|g"
fi
