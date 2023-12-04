cmake_minimum_required(VERSION 3.21)
project(pa5 C)

set(CMAKE_C_STANDARD 99)

add_compile_options(
        -pedantic
        -Wall)

add_executable(pa5
        main.c
        ipc.h
        ipc.c
        pipes.h
        pipes.c
        lamport.h
        lamport.c
        pa2345.h
        common.h
        stack.h
        stack.c
)

add_library(libruntime SHARED IMPORTED)

set_target_properties(libruntime PROPERTIES IMPORTED_LOCATION ../lib64/libruntime.so)

target_link_libraries(pa5 libruntime)