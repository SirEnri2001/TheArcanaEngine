cmake_minimum_required(VERSION 3.20)

project(SDF)
set(SDF_MODULE_ROOT ./src/application/SDF)

add_library(SDF
    ./Public/SDF.h
    ./Private/SDF.cpp
)

target_include_directories(SDF
    PRIVATE ${glm_INCLUDE_DIRS}
)

target_include_directories(SDF
    PUBLIC ./Public
    PRIVATE ./Private
)

target_link_libraries(SDF CoreMath)
