set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/XRPLF/clio")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ripple Labs Inc. <support@ripple.com>")

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA ${CMAKE_SOURCE_DIR}/cmake/pkg/postinst)

# We must replace "-" with "~" otherwise dpkg will sort "X.Y.Z-b1" as greater than "X.Y.Z"
string(REPLACE "-" "~" git "${CPACK_PACKAGE_VERSION}")
