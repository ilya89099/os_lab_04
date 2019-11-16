cmake_minimum_required(VERSION 3.14)
project(os_lab_04)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")



add_executable(os_lab_04 main.c)