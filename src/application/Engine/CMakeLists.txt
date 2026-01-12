cmake_minimum_required(VERSION 3.20)
project(Engine)

add_executable(Engine 
./Public/Engine.h
./Private/Engine.cpp)

target_include_directories(Engine
PUBLIC ./Public
)

target_link_libraries(Engine stb_image RHI CoreGeometry CoreScene CoreMath CoreLog CoreGeometry ShaderCompiler spirv-cross-hlsl)

set_target_properties(Engine PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
