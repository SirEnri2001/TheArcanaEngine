cmake_minimum_required(VERSION 3.20)

project(CoreScene)

add_library(CoreScene
./Public/CoreScene.h
./Public/CoreScene.inl
./Private/CoreScene.cpp
)

target_include_directories(CoreScene
PUBLIC ${glm_INCLUDE_DIRS}
PRIVATE ${json_INCLUDE_DIRS}
)

target_include_directories(CoreScene
PUBLIC ./Public
PRIVATE ./Private
)

target_link_libraries(CoreScene CoreGeometry CoreMath nlohmann_json)