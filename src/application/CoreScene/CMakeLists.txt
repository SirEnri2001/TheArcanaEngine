cmake_minimum_required(VERSION 3.20)

project(CoreScene)
set(CoreScene_MODULE_ROOT ./src/application/CoreScene)

add_library(CoreScene
${CoreScene_MODULE_ROOT}/Public/CoreScene.h
${CoreScene_MODULE_ROOT}/Public/CoreScene.inl
${CoreScene_MODULE_ROOT}/Private/CoreScene.cpp
)

target_include_directories(CoreScene
PUBLIC ${glm_INCLUDE_DIRS}
PRIVATE ${json_INCLUDE_DIRS}
)

target_include_directories(CoreScene
PUBLIC ${CoreScene_MODULE_ROOT}/Public
PUBLIC ${CoreMath_MODULE_ROOT}/Public
PRIVATE ${CoreScene_MODULE_ROOT}/Private
)

target_link_libraries(CoreScene CoreGeometry)