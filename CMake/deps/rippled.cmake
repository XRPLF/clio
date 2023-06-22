target_link_libraries(clio PUBLIC CONAN_PKG::clio-xrpl)
target_link_libraries(clio PUBLIC CONAN_PKG::date)
target_link_libraries(clio PUBLIC CONAN_PKG::abseil)
target_link_libraries(clio PUBLIC CONAN_PKG::grpc) 
target_link_libraries(clio PUBLIC CONAN_PKG::libuv) 
target_link_libraries(clio PUBLIC CONAN_PKG::c-ares) 
target_link_libraries(clio PUBLIC CONAN_PKG::zlib) 
target_link_libraries(clio PUBLIC CONAN_PKG::re2) 

find_package(OpenSSL 1.1.1 REQUIRED)
set_target_properties(OpenSSL::SSL PROPERTIES
  INTERFACE_COMPILE_DEFINITIONS OPENSSL_NO_SSL2
)

target_link_libraries(clio PUBLIC
  OpenSSL::Crypto
  OpenSSL::SSL
)
