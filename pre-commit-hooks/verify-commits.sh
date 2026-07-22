#!/bin/bash

# Annotated tags have object type "tag"; lightweight tags have type "commit".
# To inspect tags: git for-each-ref refs/tags
#
# To always sign commits and tags, configure:
#   git config --global commit.gpgsign true
#   git config --global tag.gpgsign true

verify_commit_signed() {
    if git verify-commit HEAD &>/dev/null; then
        echo "HEAD commit is signed."
    else
        echo "HEAD commit is not signed!"
        exit 1
    fi
}

verify_tag_annotated() {
    local version="$1"
    # git cat-file -t returns "tag" for annotated tags, "commit" for lightweight.
    if [[ "$(git cat-file -t "$version")" == "tag" ]]; then
        echo "Tag '$version' is annotated."
    else
        echo "Tag '$version' is not annotated!"
        echo "Re-create it with: git tag -a -s -m \"$version\" \"$version\""
        exit 1
    fi
}

verify_tag_signed() {
    local version="$1"
    if git verify-tag "$version" &>/dev/null; then
        echo "Tag '$version' is signed."
    else
        echo "Tag '$version' is not signed!"
        echo "Sign it with: git tag -a -s -m \"$version\" \"$version\""
        exit 1
    fi
}

# Enforce signing and annotated tags when pushing to a release branch.
if echo "$PRE_COMMIT_REMOTE_BRANCH" | grep -q "^refs/heads/release/"; then
    # git describe --exact-match guarantees a single tag; git tag --points-at HEAD
    # can return multiple newline-separated values, which breaks downstream commands.
    version=$(git describe --exact-match --tags HEAD 2>/dev/null)
    if [[ -z "$version" ]]; then
        echo "No tag found at HEAD — cannot push an untagged release commit!"
        exit 1
    fi
    echo "Looks like you're trying to push a '$version' release..."
    echo "Verifying the commit is signed and the tag is annotated and signed."
    verify_commit_signed
    verify_tag_annotated "$version"
    verify_tag_signed "$version"
fi
