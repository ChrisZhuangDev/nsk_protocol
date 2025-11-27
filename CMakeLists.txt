cmake_minimum_required(VERSION 3.22.1)
project(nsk C)

set(CMAKE_C_STANDARD 99)

add_executable(nsk main.c
        hex_ascll.c
        hex_ascll.h
        comm_protocol.c
        comm_protocol.h)
