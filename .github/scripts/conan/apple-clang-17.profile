[settings]
arch={{detect_api.detect_arch()}}
build_type=Release
compiler=apple-clang
compiler.cppstd=20
compiler.libcxx=libc++
compiler.version=17
os=Macos

[conf]
grpc/1.50.1:tools.build:cxxflags+=["-Wno-missing-template-arg-list-after-template-kw"]
