#!/usr/bin/env bash

conan profile new default --detect
conan profile update settings.compiler.cppstd=20 default
conan profile update settings.compiler.libcxx=libstdc++11 default
conan remote add --insert 0 conan-non-prod http://18.143.149.228:8081/artifactory/api/conan/conan-non-prod

cd rippled
conan export external/snappy snappy/1.1.10@
conan export external/soci soci/4.0.3@
conan export .
conan install --output-folder build_rippled -install-folder build_rippled --build missing --settings build_type=Release
cmake -B build_rippled -DCMAKE_TOOLCHAIN_FILE:FILEPATH=build/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build_rippled --target xrpl_core --parallel $(($(nproc) - 2))
cd ..

conan export external/cassandra
conan install . -if build_clio -of build_clio --build missing --build protobuf --build grpc --settings build_type=Release -o tests=True
cmake -DCMAKE_TOOLCHAIN_FILE:FILEPATH=build/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release -B build_clio
cmake --build build_clio --parallel $(($(nproc) - 2))
