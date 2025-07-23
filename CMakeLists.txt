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
add_subdirectory(./src/thirdparties)

# add custom shader compile target
include(./shaders/CMakeLists.txt)

# add modules
add_subdirectory(./src/application/CoreMath)
add_subdirectory(./src/application/CoreGeometry)
add_subdirectory(./src/application/CoreScene)
add_subdirectory(./src/application/CoreLog)
add_subdirectory(./src/application/RHI)
add_subdirectory(./src/application/SDF)
add_subdirectory(./src/application/Renderer)
add_subdirectory(./src/application/Engine)
add_subdirectory(./src/application/ShaderCompiler)