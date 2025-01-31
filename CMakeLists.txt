cmake_minimum_required(VERSION 3.20)
project(3DVulkanEngine)

set(SourceDirectory ./src)
set(ApplicationSources 
./src/application/application.cpp
)
add_executable(3DVulkanEngine ${ApplicationSources})