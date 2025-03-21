cmake_minimum_required(VERSION 3.20)

project(CoreMath)
set(CoreMath_MODULE_ROOT ./src/application/CoreMath)

add_library(CoreMath INTERFACE)

target_include_directories(CoreMath
INTERFACE ./Public)

target_link_libraries(CoreMath INTERFACE glm-header-only)
