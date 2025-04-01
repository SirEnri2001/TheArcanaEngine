cmake_minimum_required(VERSION 3.20)
project(Engine)

add_executable(Engine 
./Public/Engine.h
./Private/Engine.cpp)

target_include_directories(Engine
PUBLIC ./Public
)

add_dependencies(Engine glslCompile)

target_link_libraries(Engine CoreGeometry CoreScene Renderer CoreMath CoreLog CoreGeometry imgui)

set_target_properties(Engine PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
