cmake_minimum_required(VERSION 2.8)

project(ck-crowdnode)

set(SRC
        src/net_uuid.h
        src/net_uuid.c


        src/base64.h
        src/base64.c
        src/cJSON.h
        src/cJSON.c
        src/urldecoder.c
        src/ck-crowdnode-server.c
        )

add_executable(ck-crowdnode-server ${SRC})

IF(WIN32)

    target_link_libraries(ck-crowdnode-server ws2_32)

    install( TARGETS ck-crowdnode-server RUNTIME DESTINATION bin COMPONENT Applications)

    include(InstallRequiredSystemLibraries)

    set(CPACK_GENERATOR NSIS)
    set(CPACK_PACKAGE_NAME "ck-crowdnode-server")
    set(CPACK_PACKAGE_VENDOR "cTuning.org")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CK crowd-node server")
    set(CPACK_PACKAGE_VERSION "0.0.1")
    set(CPACK_PACKAGE_VERSION_MAJOR "0")
    set(CPACK_PACKAGE_VERSION_MINOR "0")
    set(CPACK_PACKAGE_VERSION_PATCH "1")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "CK crowd-node server")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}\\\\LICENSE.txt")
    set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}\\\\icon.ico")

    include(CPack)

ELSE(WIN32)
    target_link_libraries(ck-crowdnode-server m)
ENDIF(WIN32)
