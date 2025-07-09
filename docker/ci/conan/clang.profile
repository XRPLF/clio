[settings]
arch={{detect_api.detect_arch()}}
build_type=Release
compiler=clang
compiler.cppstd=20
compiler.libcxx=libc++
compiler.version=19
os=Linux

[conf]
tools.build:compiler_executables={"c": "/usr/bin/clang-19", "cpp": "/usr/bin/clang++-19"}
grpc/1.50.1:tools.build:cxxflags+=["-Wno-missing-template-arg-list-after-template-kw"]
