# Example usage:
#  mkdir build
#  cd build
#  cmake -g "Visual Studio 14 2015" ..

cmake_minimum_required(VERSION 3.10)
project(imgui C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 11)
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DVK_PROTOTYPES")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DVK_PROTOTYPES")

# GLFW


# Dear ImGui
set(IMGUI_DIR .)

# Libraries
find_package(Vulkan REQUIRED)
#find_library(VULKAN_LIBRARY
  #NAMES vulkan vulkan-1)
#set(LIBRARIES "glfw;${VULKAN_LIBRARY}")
set(LIBRARIES "glfw;Vulkan::Vulkan")

if(MINGW)
  # GNU ld ignores imgui's #pragma comment(lib, "dwmapi" / "gdi32") directives.
  list(APPEND LIBRARIES dwmapi gdi32)
endif()

# Use vulkan headers from glfw:
include_directories(${GLFW_DIR}/deps)

if(WIN32)
  message(STATUS "ImGUI: Target is Windows")

  add_library(imgui STATIC
          ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
          ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
          ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp
          ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
          ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp
          ${IMGUI_DIR}/imgui_demo.cpp
          ${IMGUI_DIR}/imgui_tables.cpp
          ${IMGUI_DIR}/imgui_widgets.cpp
  )
elseif(APPLE)
  message(STATUS "ImGUI: Target is macOS")

  add_library(imgui STATIC
          ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
          ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
          ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp
          ${IMGUI_DIR}/imgui_demo.cpp
          ${IMGUI_DIR}/imgui_tables.cpp
          ${IMGUI_DIR}/imgui_widgets.cpp
  )
elseif(UNIX)
  message(STATUS "ImGUI: Target is Linux/Unix-like")
endif()
# We want to statically link to ImGUI to reduce call overhead.
target_link_libraries(imgui ${LIBRARIES})

target_include_directories(imgui 
PUBLIC ${IMGUI_DIR} 
PUBLIC ${IMGUI_DIR}/backends)

