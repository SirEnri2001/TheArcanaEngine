cmake_minimum_required(VERSION 3.20)

project(CoreGeometry)

add_library(CoreGeometry
./Public/CoreGeometry.h
./Public/CoreGeometry.inl
./Private/CoreGeometry.cpp
)

target_include_directories(CoreGeometry
PUBLIC ./Public
PRIVATE ./Private
)

target_include_directories(CoreGeometry
PUBLIC ${ThirdPartiesDir}/tinyobjloader-1.0.6
)

target_link_libraries(CoreGeometry tiny_obj_loader)