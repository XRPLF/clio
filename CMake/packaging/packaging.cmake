set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Clio XRPL API server")
set(CPACK_PACKAGE_VENDOR "XRPLF")
set(CPACK_PACKAGE_CONTACT "Ripple Labs Inc. <support@ripple.com>")

set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/packages")

set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/clio")

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")

set(CPACK_PACKAGE_VERSION "${clio_version}")

if(${PKG} STREQUAL deb)
    include(CMake/packaging/deb/deb.cmake)
elseif(${PKG} STREQUAL rpm)
    include(CMake/packaging/rpm/rpm.cmake)
else()
    message(ERROR "No package type provided!")
endif()

set(CPACK_STRIP_FILES TRUE)

include(CPack)
