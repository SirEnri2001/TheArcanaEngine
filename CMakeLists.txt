cmake_minimum_required(VERSION 3.20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# specify project metadatas
project(3DVulkanEngine)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/out/library)

# find vulkan
find_package(Vulkan REQUIRED)

# specify third party dependencies
set(ThirdPartiesSource ./src/thirdparties)

add_subdirectory(${ThirdPartiesSource}/glfw-3.4)
set(glfw_IncludeDirectory ${ThirdPartiesSource}/glfw-3.4/include)

add_subdirectory(${ThirdPartiesSource}/glm-1.0.1)
set(glm_IncludeDirectory ${ThirdPartiesSource}/glm-1.0.1)

# specify targets
set(SourceDirectory ./src)
set(ApplicationSources 
${SourceDirectory}/application/application.cpp
)



source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src FILES ${ApplicationSources})
add_executable(3DVulkanEngine ${ApplicationSources})
target_include_directories(3DVulkanEngine
PUBLIC ${glfw_IncludeDirectory}
PUBLIC ${glm_IncludeDirectory}
PUBLIC ${ThirdPartiesSource}/stb_image-2.30
PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(3DVulkanEngine PRIVATE glfw ${Vulkan_LIBRARIES})

# setup vs debug working directory
set_property(TARGET 3DVulkanEngine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")