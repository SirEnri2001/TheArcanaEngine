cmake_minimum_required(VERSION 3.20)

# find vulkan
find_package(Vulkan REQUIRED)

project(RHI)
set(RHI_MODULE_ROOT ./src/application/RHI)
add_library(RHI
${RHI_MODULE_ROOT}/Public/RHI.h
${RHI_MODULE_ROOT}/Public/RHIImGuiHelper.h
${RHI_MODULE_ROOT}/Private/RHI.cpp
${RHI_MODULE_ROOT}/Private/RHIOpenGL.h
${RHI_MODULE_ROOT}/Private/RHIOpenGL.cpp
${RHI_MODULE_ROOT}/Private/RHIVulkan.h
${RHI_MODULE_ROOT}/Private/RHIVulkan.cpp
${RHI_MODULE_ROOT}/Private/RHIVulkanImpl.h
${RHI_MODULE_ROOT}/Private/RHIVulkanImpl.cpp
${RHI_MODULE_ROOT}/Private/RHIDirect3D12.h
${RHI_MODULE_ROOT}/Private/RHIDirect3D12.cpp
)

target_include_directories(RHI
PRIVATE ${glfw_INCLUDE_DIRS}
PRIVATE ${glm_INCLUDE_DIRS}
PRIVATE ${ThirdPartiesDir}/stb_image-2.30
PRIVATE ${Vulkan_INCLUDE_DIRS}
)

target_include_directories(RHI
PUBLIC ${RHI_MODULE_ROOT}/Public
PRIVATE ${RHI_MODULE_ROOT}/Private
)


target_include_directories(RHI 
PRIVATE ${CoreMath_MODULE_ROOT}/Public
PRIVATE ${CoreLog_MODULE_ROOT}/Public
)

target_link_libraries(RHI Vulkan::Vulkan imgui glfw)
target_link_libraries(RHI d3d12.lib)