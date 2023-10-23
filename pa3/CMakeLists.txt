cmake_minimum_required(VERSION 3.21)
project(pa3 C)

set(CMAKE_C_STANDARD 99)

add_compile_options(
        -pedantic
        -Wall)

add_executable(pa3
        common.h
        ipc.h
        main.c
        pa2345.h
        print_history.c
        pipes.c
        pipes.h
        ipc.c
        bank_robbery.c
        lamport.c
        lamport.h)

add_library(libruntime SHARED IMPORTED)

set_target_properties(libruntime PROPERTIES IMPORTED_LOCATION ../lib64/libruntime.so)

target_link_libraries(pa3 libruntime)