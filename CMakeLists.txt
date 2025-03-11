cmake_minimum_required(VERSION 3.20)
project(TheArcanaEngine)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# specify project metadatas
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# add CMakeLists.txt for thirdparties
include(./src/thirdparties/CMakeLists.txt)

# add custom shader compile target
include(./shaders/CMakeLists.txt)

# add modules
include(./src/application/CoreMath/CMakeLists.txt)
include(./src/application/CoreGeometry/CMakeLists.txt)
include(./src/application/CoreScene/CMakeLists.txt)
include(./src/application/CoreLog/CMakeLists.txt)
include(./src/application/RHI/CMakeLists.txt)
include(./src/application/SDF/CMakeLists.txt)
include(./src/application/Renderer/CMakeLists.txt)
include(./src/application/Engine/CMakeLists.txt)

add_dependencies(Engine glslCompile)