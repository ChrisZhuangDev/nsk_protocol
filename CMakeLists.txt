cmake_minimum_required(VERSION 3.22.1)
project(nsk C)

set(CMAKE_C_STANDARD 99)

# 添加 CMSIS-POSIX 头文件路径
include_directories(CMSIS-POSIX/inc)

# 收集 CMSIS-POSIX 源文件
file(GLOB CMSIS_POSIX_SOURCES "CMSIS-POSIX/src/*.c")

add_executable(nsk main.c
        hex_ascll.c
        hex_ascll.h
        comm_protocol.c
        comm_protocol.h
        comm_ctrl.c
        comm_ctrl.h
        fsm.c
        fsm.h
        ${CMSIS_POSIX_SOURCES})

# 链接 pthread 和 rt 库（用于 POSIX 线程和实时 timer）
target_link_libraries(nsk pthread rt)
