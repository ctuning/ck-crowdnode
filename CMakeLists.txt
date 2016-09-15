
project(CKNodeServer)
cmake_minimum_required(VERSION 2.8)

set(SRC
        cJSON.h
        cJSON.c
        CKnode.c
        )

add_executable(CKNodeServer ${SRC})
target_link_libraries(CKNodeServer m)