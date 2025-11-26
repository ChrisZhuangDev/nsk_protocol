cmake_minimum_required(VERSION 3.22.1)
project(nsk C)

set(CMAKE_C_STANDARD 99)

add_executable(nsk main.c
        lib_protocol.c
        lib_protocol.h
        protocol_parser.c
        protocol_parser.h)
