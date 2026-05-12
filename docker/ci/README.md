# CI image for XRPLF/clio

This image contains an environment to build [Clio](https://github.com/XRPLF/clio), check code and documentation.
It is used in [Clio Github Actions](https://github.com/XRPLF/clio/actions) but can also be used to compile Clio locally.

The image is based on Ubuntu 20.04 and contains:

- ccache 4.12.2
- Clang 19
- ClangBuildAnalyzer 1.6.0
- Conan 2.24.0
- Doxygen 1.16.1
- GCC 15.2.0
- GDB 17.1
- gh 2.83.2
- git-cliff 2.11.0
- LLVM Tools 21
- mold 2.40.4
- Ninja 1.13.2
- Python 3.8
- and some other useful tools

Conan is set up to build Clio without any additional steps.
There are two preset conan profiles: `clang` and `gcc` to use corresponding compiler.
`ASan`, `TSan` and `UBSan` sanitizer builds are enabled via conan profiles for each of the supported compilers.
These can be selected using the following pattern (all lowercase): `[compiler].[sanitizer]` (e.g. `--profile:all gcc.tsan`).
