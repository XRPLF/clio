# Contributing
Thank you for your interest in contributing to the `clio` project 🙏

To contribute, please:
1. Fork the repository under your own user.
2. Create a new branch on which to commit/push your changes.
3. Write and test your code.
4. Ensure that your code compiles with the provided build engine and update the provided build engine as part of your PR where needed and where appropriate.
5. Where applicable, write test cases for your code and include those in the relevant subfolder under `tests`.
6. Ensure your code passes automated checks (e.g. clang-format)
7. Squash your commits (i.e. rebase) into as few commits as is reasonable to describe your changes at a high level (typically a single commit for a small change). See below for more details.
8. Open a PR to the main repository onto the _develop_ branch, and follow the provided template.

> **Note:** Please read the [Style guide](#style-guide).

## Install git hooks
Please run the following command in order to use git hooks that are helpful for `clio` development.

``` bash
git config --local core.hooksPath .githooks
```

## Git hooks dependencies
The pre-commit hook requires `clang-format >= 18.0.0` and `cmake-format` to be installed on your machine.
`clang-format` can be installed using `brew` on macOS and default package manager on Linux.
`cmake-format` can be installed using `pip`.
The hook will also attempt to automatically use `doxygen` to verify that everything public in the codebase is covered by doc comments. If `doxygen` is not installed, the hook will raise a warning suggesting to install `doxygen` for future commits.

## Git commands
This sections offers a detailed look at the git commands you will need to use to get your PR submitted. 
Please note that there are more than one way to do this and these commands are provided for your convenience.
At this point it's assumed that you have already finished working on your feature/bug.

> **Important:** Before you issue any of the commands below, please hit the `Sync fork` button and make sure your fork's `develop` branch is up-to-date with the main `clio` repository.

``` bash
# Create a backup of your branch
git branch <your feature branch>_bk

# Rebase and squash commits into one
git checkout develop
git pull origin develop
git checkout <your feature branch>
git rebase -i develop
```
For each commit in the list other than the first one, enter `s` to squash.
After this is done, you will have the opportunity to write a message for the squashed commit.

> **Hint:** Please use **imperative mood** in the commit message, and capitalize the first word.

``` bash
# You should now have a single commit on top of a commit in `develop`
git log
```
> **Note:** If there are merge conflicts, please resolve them now.

``` bash
# Use the same commit message as you did above
git commit -m 'Your message'
git rebase --continue
```

> **Important:** If you have no GPG keys set up, please follow [this tutorial](https://docs.github.com/en/authentication/managing-commit-signature-verification/adding-a-gpg-key-to-your-github-account)

``` bash
# Sign the commit with your GPG key, and push your changes
git commit --amend -S
git push --force
```

## Use ccache (optional)
Clio uses `ccache` to speed up compilation. If you want to use it, please make sure it is installed on your machine.
CMake will automatically detect it and use it if it is available.

## Opening a pull request
When a pull request is open CI will perform checks on the new code.
Title of the pull request and squashed commit should follow [conventional commits specification](https://www.conventionalcommits.org/en/v1.0.0/).

## Fixing issues found during code review
While your code is in review, it's possible that some changes will be requested by reviewer(s).
This section describes the process of adding your fixes.

We assume that you already made the required changes on your feature branch.

``` bash
# Add the changed code
git add <paths to add>

# Add a [FOLD] commit message (so you remember to squash it later)
# while also signing it with your GPG key
git commit -S -m "[FOLD] Your commit message"

# And finally push your changes
git push
```

## After code review
When your PR is approved and ready to merge, use `Squash and merge`.
The button for that is near the bottom of the PR's page on GitHub.

> **Important:** Please leave the automatically-generated mention/link to the PR in the subject line **and** in the description field add `"Fix #ISSUE_ID"` (replacing `ISSUE_ID` with yours) if the PR fixes an issue.
> **Note:** See [issues](https://github.com/XRPLF/clio/issues) to find the `ISSUE_ID` for the feature/bug you were working on.

# Style guide
This is a non-exhaustive list of recommended style guidelines. These are not always strictly enforced and serve as a way to keep the codebase coherent.

## Formatting
Code must conform to `clang-format` version 18, unless the result would be unreasonably difficult to read or maintain.
In most cases the pre-commit hook will take care of formatting and will fix any issues automatically.
To manually format your code, use `clang-format -i <your changed files>` for C++ files and `cmake-format -i <your changed files>` for CMake files.

## Documentation
All public namespaces, classes and functions must be covered by doc (`doxygen`) comments. Everything that is not within a nested `impl` namespace is considered public.

> **Note:** Keep in mind that this is enforced by Clio's CI and your build will fail if newly added public code lacks documentation.

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
A PR must be reviewed and approved by at least one of the maintainers before it can be merged.

## Adding and Removing
New maintainers can be proposed by two existing maintainers, subject to a vote by a quorum of the existing maintainers. A minimum of 50% support and a 50% participation is required. In the event of a tie vote, the addition of the new maintainer will be rejected.

Existing maintainers can resign, or be subject to a vote for removal at the behest of two existing maintainers. A minimum of 60% agreement and 50% participation are required. The XRP Ledger Foundation will have the ability, for cause, to remove an existing maintainer without a vote.

## Existing Maintainers

* [cindyyan317](https://github.com/cindyyan317) (Ripple)
* [godexsoft](https://github.com/godexsoft) (Ripple)
* [kuznetsss](https://github.com/kuznetsss) (Ripple)
* [legleux](https://github.com/legleux) (Ripple)

## Honorable ex-Maintainers

* [cjcobb23](https://github.com/cjcobb23) (ex-Ripple)
* [natenichols](https://github.com/natenichols) (ex-Ripple)
