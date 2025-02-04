# CI image for XRPLF/clio

This image contains an environment to build [Clio](https://github.com/XRPLF/clio), check code and documentation.
It is used in [Clio Github Actions](https://github.com/XRPLF/clio/actions) but can also be used to compile Clio locally.

The image is based on Ubuntu 20.04 and contains:
- clang 16.0.6
- gcc 12.3
- doxygen 1.12
- gh 2.40
- ccache 4.10.2
- conan 1.62
- and some other useful tools

Conan is set up to build Clio without any additional steps. There are two preset conan profiles: `clang` and `gcc` to use corresponding compiler. By default conan is setup to use `gcc`.
Sanitizer builds for `ASAN`, `TSAN` and `UBSAN` are enabled via conan profiles for each of the supported compilers. These can be selected using the following pattern (all lowercase): `[compiler].[sanitizer]` (e.g. `--profile gcc.tsan`). 
