find_package (OpenSSL 1.1.1 REQUIRED)

set_target_properties (OpenSSL::SSL PROPERTIES
  INTERFACE_COMPILE_DEFINITIONS OPENSSL_NO_SSL2
)
