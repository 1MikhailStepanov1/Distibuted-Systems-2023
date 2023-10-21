cmake_minimum_required(VERSION 3.21)
project(pa1 C)

set(CMAKE_C_STANDARD 99)

add_compile_options(
        -pedantic
        -Wall)

add_executable(pa1
        common.h
        ipc.h
        main.c
        pa1.h)
