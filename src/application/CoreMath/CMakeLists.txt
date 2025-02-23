cmake_minimum_required(VERSION 3.20)

project(CoreMath)
set(CoreMath_MODULE_ROOT ./src/application/CoreMath)

add_library(CoreMath
${CoreMath_MODULE_ROOT}/Public/CoreMath.h
${CoreMath_MODULE_ROOT}/Public/CoreMath.inl
${CoreMath_MODULE_ROOT}/Private/CoreMath.cpp
)

target_include_directories(CoreMath
PUBLIC ${glm_INCLUDE_DIRS})

target_include_directories(CoreMath
PUBLIC ${CoreMath_MODULE_ROOT}/Public
PRIVATE ${CoreMath_MODULE_ROOT}/Private
)