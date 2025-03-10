cmake_minimum_required(VERSION 3.20)

project(SDF)
set(SDF_MODULE_ROOT ./src/application/SDF)

add_library(SDF
    ${SDF_MODULE_ROOT}/Public/SDF.h
    ${SDF_MODULE_ROOT}/Private/SDF.cpp
)

target_include_directories(SDF
    PRIVATE ${glm_INCLUDE_DIRS}
)

target_include_directories(SDF
    PUBLIC ${SDF_MODULE_ROOT}/Public
    PRIVATE ${SDF_MODULE_ROOT}/Private
)

target_include_directories(SDF 
    PRIVATE ${CoreMath_MODULE_ROOT}/Public
)

