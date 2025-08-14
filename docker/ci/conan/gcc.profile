[settings]
arch={{detect_api.detect_arch()}}
build_type=Release
compiler=gcc
compiler.cppstd=20
compiler.libcxx=libstdc++11
compiler.version=15
os=Linux

[conf]
tools.build:compiler_executables={"c": "/usr/bin/gcc-15", "cpp": "/usr/bin/g++-15"}
