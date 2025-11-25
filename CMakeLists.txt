cmake_minimum_required(VERSION 3.31)
project(nsk C)

set(CMAKE_C_STANDARD 99)

add_executable(nsk main.c
        lib_protocol.c)
