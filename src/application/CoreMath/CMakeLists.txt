cmake_minimum_required(VERSION 3.20)

project(CoreMath)
set(CoreMath_MODULE_ROOT ./src/application/CoreMath)

add_library(CoreMath INTERFACE)

target_include_directories(CoreMath
INTERFACE ${glm_INCLUDE_DIRS})

target_include_directories(CoreMath
INTERFACE ./Public
)