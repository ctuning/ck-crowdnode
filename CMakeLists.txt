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
    target_link_libraries(ck-crowdnode-server ws2_32 m)
ELSE(WIN32)
    target_link_libraries(ck-crowdnode-server  m)
ENDIF(WIN32)
