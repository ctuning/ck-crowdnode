cmake_minimum_required(VERSION 2.8)

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

    set(CPACK_GENERATOR WIX)
    set(CPACK_PACKAGE_NAME "ck-crowdnode-server")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CK crowd-node server")
    set(CPACK_PACKAGE_VERSION "0.0.1")

    INCLUDE(CPack)

ELSE(WIN32)
    target_link_libraries(ck-crowdnode-server m)
ENDIF(WIN32)
