cmake_minimum_required(VERSION 3.20)

project(Renderer)
set(Renderer_MODULE_ROOT ./src/application/Renderer)

add_library(Renderer
${Renderer_MODULE_ROOT}/Public/Renderer.h
${Renderer_MODULE_ROOT}/Private/Renderer.cpp
)

target_include_directories(Renderer
PUBLIC ${Renderer_MODULE_ROOT}/Public
PUBLIC ${CoreLog_MODULE_ROOT}/Public
)

target_link_libraries(Renderer RHI CoreScene)