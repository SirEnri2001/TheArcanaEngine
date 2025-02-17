cmake_minimum_required(VERSION 3.20)

# find vulkan
find_package(Vulkan REQUIRED)

project(RHI)
set(RHI_MODULE_ROOT ./src/application/RHI)
add_library(RHI
${RHI_MODULE_ROOT}/Public/RHI.h
${RHI_MODULE_ROOT}/Public/RHIOpenGL.h
${RHI_MODULE_ROOT}/Public/RHIVulkan.h
${RHI_MODULE_ROOT}/Private/RHI.cpp
${RHI_MODULE_ROOT}/Private/RHIOpenGL.cpp
${RHI_MODULE_ROOT}/Private/RHIVulkan.cpp
)

target_include_directories(RHI
PRIVATE ${glfw_INCLUDE_DIRS}
PRIVATE ${glm_INCLUDE_DIRS}
PRIVATE ${ThirdPartiesDir}/stb_image-2.30
PRIVATE ${ThirdPartiesDir}/tinyobjloader-1.0.6
PRIVATE ${Vulkan_INCLUDE_DIRS})

target_include_directories(RHI
PUBLIC ${RHI_MODULE_ROOT}/Public
)

target_link_libraries(RHI Vulkan::Vulkan)
target_link_libraries(RHI glfw)