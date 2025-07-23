cmake_minimum_required(VERSION 3.20)

project(ShaderCompiler)
set(ShaderCompiler_MODULE_ROOT ./src/application/ShaderCompiler)

add_library(ShaderCompiler
./Public/ShaderCompiler.h
./Private/ShaderCompiler.cpp
)

target_include_directories(ShaderCompiler
PUBLIC ./Public
PUBLIC ${CoreLog_MODULE_ROOT}/Public
)
