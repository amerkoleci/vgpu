cmake_minimum_required(VERSION 3.0)
project(spirv_reflect)

add_library(spirv_reflect STATIC
    ${CMAKE_CURRENT_LIST_DIR}/spirv_reflect.h
    ${CMAKE_CURRENT_LIST_DIR}/spirv_reflect.c
)
