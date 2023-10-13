set(CPACK_GENERATOR "RPM")
# set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
set(CPACK_SYSTEM_NAME "x86_64")

set(CPACK_RPM_USER_FILELIST
    "%config /lib/systemd/system/clio.service"
    "%config /opt/clio/bin/clio_server"
    "%config /opt/clio/etc/config.json"
)

set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
/lib
/lib/systemd
/lib/systemd/system
/opt
)

# #create a directory for the symlinks to be created
set(symlink_directory "${CMAKE_CURRENT_BINARY_DIR}/symlink")
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${symlink_directory})

# #symlink for clio_server
execute_process(COMMAND ${CMAKE_COMMAND}
    -E create_symlink /opt/clio/bin/clio_server ${symlink_directory}/clio_server)
install(FILES ${symlink_directory}/clio_server DESTINATION /usr/local/bin)


set(CPACK_RPM_PACKAGE_RELEASE_DIST "el7")
string(REPLACE "-" ";" VERSION_LIST ${CPACK_PACKAGE_VERSION})
list(GET VERSION_LIST 0 CPACK_RPM_PACKAGE_VERSION)

message("CPACK_RPM_PACKAGE_VERSION ${CPACK_RPM_PACKAGE_VERSION}")
message("VERSION_LIST ${VERSION_LIST}")
list (LENGTH VERSION_LIST _len)

if (${_len} GREATER 1)
    list(GET VERSION_LIST 1 PRE_VERSION)
    message("PRE_VERSION: ${PRE_VERSION}")
    set(CPACK_RPM_PACKAGE_RELEASE "0.1${CPACK_RPM_PACKAGE_RELEASE}.${PRE_VERSION}")
else()
    set(CPACK_RPM_PACKAGE_RELEASE "1${CPACK_RPM_PACKAGE_RELEASE}")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
endif()
