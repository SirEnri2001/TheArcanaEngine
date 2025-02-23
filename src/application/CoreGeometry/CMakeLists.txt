cmake_minimum_required(VERSION 3.20)

project(CoreGeometry)
set(CoreGeometry_MODULE_ROOT ./src/application/CoreGeometry)

add_library(CoreGeometry
${CoreGeometry_MODULE_ROOT}/Public/CoreGeometry.h
${CoreGeometry_MODULE_ROOT}/Public/CoreGeometry.inl
${CoreGeometry_MODULE_ROOT}/Private/CoreGeometry.cpp
)

target_include_directories(CoreGeometry
PUBLIC ${CoreGeometry_MODULE_ROOT}/Public
PRIVATE ${CoreGeometry_MODULE_ROOT}/Private
)