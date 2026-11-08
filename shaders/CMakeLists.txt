cmake_minimum_required(VERSION 3.20)

# ------------------------------------------------------------------ paths ---
# This file is included by the top-level CMakeLists via include(), so
# CMAKE_CURRENT_LIST_DIR is the shaders/ directory and CMAKE_SOURCE_DIR is
# the project root (the engine's runtime working directory).
set(SHADER_SOURCE_DIR          ${CMAKE_CURRENT_LIST_DIR})
set(SHADER_BYTECODE_DIRECTORY  ${CMAKE_SOURCE_DIR}/shaderbytecode)
set(SHADER_HLSL_GENERATED_DIR  ${CMAKE_SOURCE_DIR}/hlsl_generated)

file(MAKE_DIRECTORY ${SHADER_BYTECODE_DIRECTORY}/glsl)
file(MAKE_DIRECTORY ${SHADER_BYTECODE_DIRECTORY}/hlsl)
file(MAKE_DIRECTORY ${SHADER_HLSL_GENERATED_DIR}/glsl)

# ------------------------------------------------------------------ tools ---
# glslc ships with the Vulkan SDK. A user may override it with
# -DGLSLC_EXECUTABLE=<path>.
if(NOT GLSLC_EXECUTABLE)
    find_program(GLSLC_EXECUTABLE NAMES glslc glslc.exe
        HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VK_SDK_PATH}/Bin")
endif()

# DXC_EXE_PATH is a CACHE variable resolved in src/thirdparties/CMakeLists.txt:
# user override -> vendored dxc_2025_02_20 -> system dxc (Vulkan SDK).
# Use $<TARGET_FILE:...> instead of a hardcoded bin/Debug path so this works
# for both single-config (MinGW Makefiles) and multi-config (Visual Studio).
set(SPIRV_CROSS_BIN $<TARGET_FILE:spirv-cross>)

# ----------------------------------------------------------- GLSL sources ---
# Patterns must be absolute paths rooted at shaders/; the original used
# "/glsl/*.frag" which matched nothing and made glslCompile a no-op.
file(GLOB_RECURSE GLSL_SHADER_SOURCES
    RELATIVE ${SHADER_SOURCE_DIR}
    CONFIGURE_DEPENDS
    "${SHADER_SOURCE_DIR}/glsl/*.vert"
    "${SHADER_SOURCE_DIR}/glsl/*.frag"
    "${SHADER_SOURCE_DIR}/glsl/*.comp")
file(GLOB_RECURSE GLSL_INCLUDE_SOURCES CONFIGURE_DEPENDS
    "${SHADER_SOURCE_DIR}/glsl/*.glsl")

set(GLSL_SPV_OUTPUTS "")
if(GLSLC_EXECUTABLE)
    foreach(SHADER_FILE ${GLSL_SHADER_SOURCES})
        set(SPV_FILE "${SHADER_BYTECODE_DIRECTORY}/${SHADER_FILE}.spv")
        get_filename_component(SPV_DIR "${SPV_FILE}" DIRECTORY)
        add_custom_command(
            OUTPUT "${SPV_FILE}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SPV_DIR}"
            COMMAND "${GLSLC_EXECUTABLE}"
                    "-I${SHADER_SOURCE_DIR}/glsl"
                    "${SHADER_SOURCE_DIR}/${SHADER_FILE}"
                    -o "${SPV_FILE}"
            DEPENDS "${SHADER_SOURCE_DIR}/${SHADER_FILE}" ${GLSL_INCLUDE_SOURCES}
            WORKING_DIRECTORY "${SHADER_SOURCE_DIR}"
            COMMENT "glslc: ${SHADER_FILE} -> ${SHADER_FILE}.spv"
            VERBATIM)
        list(APPEND GLSL_SPV_OUTPUTS "${SPV_FILE}")
    endforeach()
else()
    message(WARNING "glslc not found. Install the Vulkan SDK or pass -DGLSLC_EXECUTABLE=<path>; GLSL SPIR-V will not be generated.")
endif()

# Runtime bytecode: part of the default build.
if(GLSL_SPV_OUTPUTS)
    add_custom_target(glslCompile ALL DEPENDS ${GLSL_SPV_OUTPUTS})
else()
    add_custom_target(glslCompile ALL)
endif()

# ----------------------------------------------- SPIR-V -> HLSL (optional) ---
# Transpiled HLSL is a debugging aid, not a runtime asset, so this target is
# explicit (make hlslGenerate) rather than part of ALL.
set(HLSL_GENERATED_OUTPUTS "")
foreach(SHADER_FILE ${GLSL_SHADER_SOURCES})
    set(SPV_FILE "${SHADER_BYTECODE_DIRECTORY}/${SHADER_FILE}.spv")
    list(FIND GLSL_SPV_OUTPUTS "${SPV_FILE}" _spv_index)
    if(NOT _spv_index EQUAL -1)
        set(HLSL_FILE "${SHADER_HLSL_GENERATED_DIR}/${SHADER_FILE}.hlsl")
        get_filename_component(HLSL_DIR "${HLSL_FILE}" DIRECTORY)
        add_custom_command(
            OUTPUT "${HLSL_FILE}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${HLSL_DIR}"
            COMMAND "${SPIRV_CROSS_BIN}" "${SPV_FILE}"
                    --output "${HLSL_FILE}" --hlsl --shader-model 40
            DEPENDS "${SPV_FILE}" spirv-cross
            COMMENT "spirv-cross: ${SHADER_FILE}.spv -> HLSL"
            VERBATIM)
        list(APPEND HLSL_GENERATED_OUTPUTS "${HLSL_FILE}")
    endif()
endforeach()

if(HLSL_GENERATED_OUTPUTS)
    add_custom_target(hlslGenerate DEPENDS ${HLSL_GENERATED_OUTPUTS})
else()
    add_custom_target(hlslGenerate)
endif()

# ------------------------------------------------------------- HLSL -> SPIR-V ---
set(HLSL_SHADERS
    "hlsl/PBRVert.hlsl|vs_6_0"
    "hlsl/PBRFrag.hlsl|ps_6_0")
file(GLOB HLSL_INCLUDE_SOURCES CONFIGURE_DEPENDS
    "${SHADER_SOURCE_DIR}/hlsl/*.hlsli"
    "${SHADER_SOURCE_DIR}/hlsl/*.hlsl")

set(HLSL_SPV_OUTPUTS "")
if(DXC_EXE_PATH)
    foreach(ENTRY ${HLSL_SHADERS})
        string(REPLACE "|" ";" _parts "${ENTRY}")
        list(GET _parts 0 SHADER_FILE)
        list(GET _parts 1 SHADER_PROFILE)

        get_filename_component(SHADER_NAME "${SHADER_FILE}" NAME_WLE)
        set(SPV_FILE "${SHADER_BYTECODE_DIRECTORY}/hlsl/${SHADER_NAME}Debug.spv")
        get_filename_component(SPV_DIR "${SPV_FILE}" DIRECTORY)

        add_custom_command(
            OUTPUT "${SPV_FILE}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SPV_DIR}"
            COMMAND "${DXC_EXE_PATH}"
                    -spirv -Zi -T ${SHADER_PROFILE} -E main
                    "-I${SHADER_SOURCE_DIR}/hlsl"
                    "${SHADER_SOURCE_DIR}/${SHADER_FILE}"
                    -Fo "${SPV_FILE}"
            DEPENDS "${SHADER_SOURCE_DIR}/${SHADER_FILE}" ${HLSL_INCLUDE_SOURCES}
            WORKING_DIRECTORY "${SHADER_SOURCE_DIR}"
            COMMENT "dxc: ${SHADER_FILE} (${SHADER_PROFILE}) -> ${SHADER_NAME}Debug.spv"
            VERBATIM)
        list(APPEND HLSL_SPV_OUTPUTS "${SPV_FILE}")
    endforeach()
else()
    message(WARNING "DXC_EXE_PATH is empty. Set -DDXC_EXE_PATH=<dxc.exe> or run src/thirdparties/dxc_2025_02_20/DXC_downloader.bat; HLSL SPIR-V will not be generated.")
endif()

if(HLSL_SPV_OUTPUTS)
    add_custom_target(hlslCompile ALL DEPENDS ${HLSL_SPV_OUTPUTS})
else()
    add_custom_target(hlslCompile)
endif()
