cmake_minimum_required(VERSION 3.22.1)
project(nsk C)

set(CMAKE_C_STANDARD 99)

# 添加 CMSIS-POSIX 头文件路径
include_directories(../../)


add_executable(nsk main.c
        ../../hex_ascll.c
        ../../hex_ascll.h
        ../../comm_protocol.c
        ../../comm_protocol.h)
