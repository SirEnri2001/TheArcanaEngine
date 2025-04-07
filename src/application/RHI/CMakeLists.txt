cmake_minimum_required(VERSION 3.20)

# find vulkan
find_package(Vulkan REQUIRED)

project(RHI)
# set(RHI_MODULE_ROOT ./src/application/RHI)
add_library(RHI
./Public/RHI.h
./Public/RHIImGuiHelper.h
./Private/RHI.cpp
./Private/RHIOpenGL.h
./Private/RHIOpenGL.cpp

# Vulkan Implementation for RHI
./Private/Vulkan/RHIVulkan.h
./Private/Vulkan/RHIVulkan.cpp
./Private/Vulkan/RHIVulkanImpl.h
./Private/Vulkan/RHIVulkanImpl.cpp
./Private/Vulkan/RHIVulkanResource.h
./Private/Vulkan/RHIVulkanResource.cpp
./Private/Vulkan/RHIVulkanGraphics.h
./Private/Vulkan/RHIVulkanGraphics.cpp

# D3D12 Implementation for RHI
./Private/RHID3D12.h
./Private/RHID3D12.cpp
./Private/RHID3D12Impl.h
./Private/RHID3D12Impl.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Private/Vulkan PREFIX "Implementations/Vulkan"
FILES
# Vulkan Implementation for RHI
./Private/Vulkan/RHIVulkan.h
./Private/Vulkan/RHIVulkan.cpp
./Private/Vulkan/RHIVulkanImpl.h
./Private/Vulkan/RHIVulkanImpl.cpp
./Private/Vulkan/RHIVulkanResource.h
./Private/Vulkan/RHIVulkanResource.cpp
./Private/Vulkan/RHIVulkanGraphics.h
./Private/Vulkan/RHIVulkanGraphics.cpp
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Private PREFIX "Implementations/D3D12"
FILES
# D3D12 Implementation for RHI
./Private/RHIDirect3D12.h
./Private/RHIDirect3D12.cpp
)

target_include_directories(RHI
PRIVATE ${glfw_INCLUDE_DIRS}
PRIVATE ${glm_INCLUDE_DIRS}
PRIVATE ${Vulkan_INCLUDE_DIRS}
)

target_include_directories(RHI
PUBLIC ./Public
PRIVATE ./Private
)

add_dependencies(RHI CoreMath CoreLog)

target_link_libraries(RHI Vulkan::Vulkan imgui glfw CoreLog  DirectX-Headers d3d12 dxgi d3dcompiler dxguid)
target_link_libraries(RHI d3d12.lib)