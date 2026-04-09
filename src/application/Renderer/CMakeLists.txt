cmake_minimum_required(VERSION 3.20)

project(Renderer)
set(Renderer_MODULE_ROOT ./src/application/Renderer)

add_library(Renderer
./Public/Renderer.h
./Private/Renderer.cpp
./Public/PBRRenderer.h
./Private/PBRRenderer.cpp
./Public/PathTraceRenderer.h
./Private/PathTraceRenderer.cpp
)

target_include_directories(Renderer
PUBLIC ./Public
PUBLIC ${CoreLog_MODULE_ROOT}/Public
)
add_dependencies(Renderer stb_image RHI ShaderCompiler)
target_link_libraries(Renderer RHI CoreScene stb_image ShaderCompiler)