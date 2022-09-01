# Contributing
Thank you for your interest in contributing to the `clio` project 🙏

To contribute, please:
1. Fork the repository under your own user.
2. Create a new branch on which to write your changes.
3. Write and test your code.
4. Ensure that your code compiles with the provided build engine and update the provided build engine as part of your PR where needed and where appropriate.
5. Where applicable, write test cases for your code and include those in `unittests`.
6. Ensure your code passes automated checks (e.g. clang-format)
7. Squash your commits (i.e. rebase) into as few commits as is reasonable to describe your changes at a high level (typically a single commit for a small change.). See below for more details.
8. Open a PR to the main repository onto the _develop_ branch, and follow the provided template.

> **Note:** Please make sure you read the [Style guide](#style-guide).

## Git commands
This sections offers a detailed look at the git commands you will need to use to get your PR submitted. 
Please note that there are more than one way to do this and these commands are only  provided for your convenience.
At this point it's assumed that you have already finished working on your feature/bug.

> **Important:** Before you issue any of the commands below, please hit the `Sync fork` button and make sure your fork's `develop` branch is up to date with the main `clio` repository.

``` bash
# Create a backup of your branch
git branch <your feature branch>_bk

# Rebase and squash commits into one
git checkout develop
git pull origin develop
git checkout <your feature branch>
git rebase -i develop
```
For each commit in the list other than the first one please select `s` to squash.
After this is done you will have the opportunity to write a message for the squashed commit:

> **Note:** See [issues](https://github.com/XRPLF/clio/issues) for the `ISSUE_ID`
> **Important:** Please use **imperative mood** commit message in the following format: `"Your message (#ISSUE_ID)"` (capitalized, imperative mood, issue_id provided at the end).

``` bash
# You should now have a single commit on top of a commit in `develop`
git log
```
> **Todo:** In case there are merge conflicts, please resolve them now

``` bash
# Use the same commit message as you did above
git commit -m 'Your message (#ISSUE_ID)'
git rebase --continue
```

> **Important:** If you have no GPG keys setup please follow [this tutorial](https://docs.github.com/en/authentication/managing-commit-signature-verification/adding-a-gpg-key-to-your-github-account)

``` bash
# Sign the commit with your GPG key and finally push your changes to the repo
git commit --amend -S
git push --force
```

## Fixing issues found during code review
While your code is in review it's possible that some changes will be requested by the reviewer.
This section describes the process of adding your fixes.

We assume that you already made the required changes on your feature branch.

``` bash
# Add the changed code
git add <paths to add>

# Add a folded commit message (so you can squash them later)
# while also signing it with your GPG key
git commit -S -m "[FOLD] Your commit message"

# And finally push your changes
git push --force
```
## After code review
Last but not least, when your PR is approved you still have to `Squash and merge` your code. 
Luckily there is a button for that towards the bottom of the PR's page on github.

# Style guide
This is a non-exhaustive list of recommended style guidelines. These are not always strictly enforced and serve as a way to keep the codebase coherent rather than a set of _thou shalt not_ commandments.

## Formatting
All code must conform to `clang-format` version 10, unless the result would be unreasonably difficult to read or maintain.
To change your code to conform use `clang-format -i <your changed files>`.

## Avoid
* Proliferation of nearly identical code.
* Proliferation of new files and classes unless it improves readability or/and compilation time.
* Unmanaged memory allocation and raw pointers.
* Macros (unless they add significant value.)
* Lambda patterns (unless these add significant value.)
* CPU or architecture-specific code unless there is a good reason to include it, and where it is used guard it with macros and provide explanatory comments.
* Importing new libraries unless there is a very good reason to do so.

## Seek to
* Extend functionality of existing code rather than creating new code.
* Prefer readability over terseness where important logic is concerned.
* Inline functions that are not used or are not likely to be used elsewhere in the codebase.
* Use clear and self-explanatory names for functions, variables, structs and classes.
* Use TitleCase for classes, structs and filenames, camelCase for function and variable names, lower case for namespaces and folders.
* Provide as many comments as you feel that a competent programmer would need to understand what your code does.

# Maintainers
Maintainers are ecosystem participants with elevated access to the repository. They are able to push new code, make decisions on when a release should be made, etc.

## Code Review
PRs must be reviewed by at least one of the maintainers.

## Adding and Removing
New maintainers can be proposed by two existing maintainers, subject to a vote by a quorum of the existing maintainers. A minimum of 50% support and a 50% participation is required. In the event of a tie vote, the addition of the new maintainer will be rejected.

Existing maintainers can resign, or be subject to a vote for removal at the behest of two existing maintainers. A minimum of 60% agreement and 50% participation are required. The XRP Ledger Foundation will have the ability, for cause, to remove an existing maintainer without a vote.

## Existing Maintainers

* [cjcobb23](https://github.com/cjcobb23) (Ripple)
* [natenichols](https://github.com/natenichols) (Ripple)
* [legleux](https://github.com/legleux) (Ripple)
* [undertome](https://github.com/undertome) (Ripple)
* [godexsoft](https://github.com/godexsoft) (Ripple)
