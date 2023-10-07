cmake_minimum_required(VERSION 3.21)
project(Distibuted_Systems_2023 C)

set(CMAKE_C_STANDARD 99)

add_compile_options(
        -pedantic
        -Wall)

include_directories(pa1)

add_executable(Distibuted_Systems_2023
        pa1/common.h
        pa1/ipc.h
        pa1/main.c
        pa1/pa1.h)
