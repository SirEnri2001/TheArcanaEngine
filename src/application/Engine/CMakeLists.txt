cmake_minimum_required(VERSION 3.20)
project(Engine)
set(ENGINE_MODULE_ROOT ./src/application/Engine)

add_executable(Engine 
${ENGINE_MODULE_ROOT}/Public/Engine.h
${ENGINE_MODULE_ROOT}/Private/Engine.cpp)
target_include_directories(Engine
PUBLIC ${ENGINE_MODULE_ROOT}/Public
PRIVATE ${ThirdPartiesDir}/tinyobjloader-1.0.6
)

target_link_libraries(Engine RHI)
target_include_directories(Engine PRIVATE ${RHI_MODULE_ROOT}/Public)
target_include_directories(Engine 
PUBLIC ${CoreMath_MODULE_ROOT}/Public
PUBLIC ${glm_INCLUDE_DIRS}
)

set_target_properties(Engine PROPERTIES 
    VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})