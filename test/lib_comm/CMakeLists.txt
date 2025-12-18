cmake_minimum_required(VERSION 3.22.1)
project(nsk C)

set(CMAKE_C_STANDARD 99)
set(LIB_DIR ${CMAKE_SOURCE_DIR}/../../)
# 添加 CMSIS-POSIX 头文件路径
include_directories(${LIB_DIR})
include_directories(${LIB_DIR}/CMSIS-POSIX/inc)
# 收集 CMSIS-POSIX 源文件
file(GLOB CMSIS_POSIX_SOURCES "${CMAKE_SOURCE_DIR}/../../CMSIS-POSIX/src/*.c")

add_executable(nsk 
        main.c
        ${LIB_DIR}/hex_ascll.c
        ${LIB_DIR}/hex_ascll.h
        ${LIB_DIR}/message.c
        ${LIB_DIR}/message.h
        ${LIB_DIR}/comm_protocol.c
        ${LIB_DIR}/comm_protocol.h
        ${LIB_DIR}/comm_ctrl.c
        ${LIB_DIR}/comm_ctrl.h
        ${LIB_DIR}/fsm.c
        ${LIB_DIR}/fsm.h
        ${LIB_DIR}/drv_socket.c
        ${LIB_DIR}/drv_socket.h
        ${LIB_DIR}/lib_comm.c
        ${LIB_DIR}/lib_comm.h
        ${CMSIS_POSIX_SOURCES})
