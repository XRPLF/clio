[settings]
arch={{detect_api.detect_arch()}}
build_type=Release
compiler=gcc
compiler.cppstd=20
compiler.libcxx=libstdc++11
compiler.version=14
os=Linux

[conf]
tools.build:compiler_executables={"c": "/usr/bin/gcc-14", "cpp": "/usr/bin/g++-14"}
