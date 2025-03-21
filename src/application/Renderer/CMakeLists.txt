cmake_minimum_required(VERSION 3.20)

project(Renderer)
set(Renderer_MODULE_ROOT ./src/application/Renderer)

add_library(Renderer
./Public/Renderer.h
./Private/Renderer.cpp
)

target_include_directories(Renderer
PUBLIC ./Public
PUBLIC ${CoreLog_MODULE_ROOT}/Public
)

target_link_libraries(Renderer RHI CoreScene)