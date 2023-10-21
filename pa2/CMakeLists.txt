cmake_minimum_required(VERSION 3.21)
project(pa2 C)

set(CMAKE_C_STANDARD 99)

add_compile_options(
        -pedantic
        -Wall)

add_executable(pa2
        common.h
        ipc.h
        main.c
        pa2345.h
        print_history.c
        pipes.c
        pipes.h
        ipc.c)
