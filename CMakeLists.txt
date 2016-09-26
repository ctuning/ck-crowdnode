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


#        src/libuuid/uuid.h
#        src/libuuid/clear.c
#        src/libuuid/compare.c
#        src/libuuid/copy.c
#        src/libuuid/gen_uuid.c
#        src/libuuid/isnull.c
#        src/libuuid/pack.c
#        src/libuuid/parse.c
#        src/libuuid/test_uuid.c
#        src/libuuid/unpack.c
#        src/libuuid/unparse.c
#        src/libuuid/uuid_time.c
#        src/libuuid/uuidd.h
#        src/libuuid/uuidP.h

        src/net_uuid.c src/net_uuid.h)

add_executable(ck-crowdnode-server ${SRC})

IF(WIN32)
    target_link_libraries(ck-crowdnode-server ws2_32 m)
ELSE(WIN32)
    target_link_libraries(ck-crowdnode-server  m)
ENDIF(WIN32)
