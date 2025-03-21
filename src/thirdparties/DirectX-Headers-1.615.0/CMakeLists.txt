# Copyright (c) Microsoft Corporation.
# Licensed under the MIT License.
cmake_minimum_required(VERSION 3.10.2)
project(DirectX-Headers
    LANGUAGES CXX
    VERSION 1.615.0
)
include(CTest)
set(CMAKE_CXX_STANDARD 14) 
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
enable_testing()

# It's useful to know if you are a top level project or not, if your project is
# being consumed via add_subdirectory
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(IS_TOPLEVEL_PROJECT TRUE)
else()
    set(IS_TOPLEVEL_PROJECT FALSE)
endif()

# Use DXHEADERS_* prefix to avoid potential name conflicts in cmake-gui, and allow
# grouping by prefix if more options are added
#
# Testing should only be enabled by default if we are top level. Otherwise clients can set it
# either via cmake or cmake-gui

include(GNUInstallDirs)

# Enables consumers to add this library as a link target to automatically add
# these include directories, regardless of whether this is referenced via subdirectory
# or from an installed location
add_library(DirectX-Headers STATIC src/d3dx12_property_format_table.cpp)
target_include_directories(DirectX-Headers SYSTEM PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_include_directories(DirectX-Headers PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/directx)

add_library(Microsoft::DirectX-Headers ALIAS DirectX-Headers)

add_library(DirectX-Guids STATIC src/dxguids.cpp)
target_link_libraries(DirectX-Guids PRIVATE DirectX-Headers)

add_library(Microsoft::DirectX-Guids ALIAS DirectX-Guids)

# For non-Windows targets, also add the WSL stubs to the include path
if (NOT WIN32)
    target_include_directories(DirectX-Headers SYSTEM PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/wsl/stubs>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/wsl/stubs>"
    )
elseif((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    # MinGW has RPC headers which define old versions, and complain if D3D
    # headers are included before the RPC headers, since D3D headers were
    # generated with new MIDL and "require" new RPC headers.
   target_compile_definitions(DirectX-Headers PRIVATE "__REQUIRED_RPCNDR_H_VERSION__=475")
   target_compile_definitions(DirectX-Guids PRIVATE "__REQUIRED_RPCNDR_H_VERSION__=475")
endif()
