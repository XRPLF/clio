#!/bin/bash

# git for-each-ref refs/tags  # see which tags are annotated and which are lightweight. Annotated tags are "tag" objects.
# # Set these so your commits and tags are always signed
# git config commit.gpgsign true
# git config tag.gpgsign true

verify_commit_signed() {
    if git verify-commit HEAD &>/dev/null; then
        :
        # echo "HEAD commit seems signed..."
    else
        echo "HEAD commit isn't signed!"
        exit 1
    fi
}

verify_tag() {
    if git describe --exact-match --tags HEAD &>/dev/null; then
        : # You might be ok to push
        # echo "Tag is annotated."
        return 0
    else
        echo "Tag for [$version] not an annotated tag."
        exit 1
    fi
}

verify_tag_signed() {
    if git verify-tag "$version" &>/dev/null; then
        : # ok, I guess we'll let you push
        # echo "Tag appears signed"
        return 0
    else
        echo "$version tag isn't signed"
        echo "Sign it with [git tag -ams\"$version\" $version]"
        exit 1
    fi
}

# Check some things if we're pushing a branch called "release/"
if echo "$PRE_COMMIT_REMOTE_BRANCH" | grep ^refs\/heads\/release\/ &>/dev/null; then
    version=$(git tag --points-at HEAD)
    echo "Looks like you're trying to push a $version release..."
    echo "Making sure you've signed and tagged it."
    if verify_commit_signed && verify_tag && verify_tag_signed; then
        : # Ok, I guess you can push
    else
        exit 1
    fi
fi
