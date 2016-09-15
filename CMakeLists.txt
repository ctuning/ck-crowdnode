
project(CKNodeServer)
cmake_minimum_required(VERSION 2.8)

set(SRC
        src/cJSON.h
        src/cJSON.c
        src/ck-crowdnode-server.c
        )

add_executable(ck-crowdnode-server ${SRC})
target_link_libraries(ck-crowdnode-server m)