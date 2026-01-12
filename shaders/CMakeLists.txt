cmake_minimum_required(VERSION 3.20)

# set paths
set(SHADER_BYTECODE_DIRECTORY ${CMAKE_SOURCE_DIR}/shaderbytecode)
set(SHADER_HLSL_GENERATED_DIR ${CMAKE_SOURCE_DIR}/hlsl_generated)
set(SPIRV_CROSS_BIN ${CMAKE_BINARY_DIR}/bin/Debug/spirv-cross.exe)


file(GLOB_RECURSE SHADER_SOURCES RELATIVE ${CMAKE_SOURCE_DIR}/shaders "*/glsl/*.frag" "*/glsl/*.vert" "*/glsl/*.comp")
file(GLOB_RECURSE SHADER_SOURCES_ABS "*/shaders/glsl/*.frag" "*/shaders/glsl/*.vert" "*/shaders/glsl/*.comp")

# ---- custom target: glslCompile ----
make_directory(${SHADER_BYTECODE_DIRECTORY}/glsl/)
add_custom_target(glslCompile 
DEPENDS ${SHADER_SOURCES}
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders
)

foreach(SHADER_FILE ${SHADER_SOURCES})
    message(WARNING ${SHADER_FILE})
    add_custom_command(TARGET glslCompile  POST_BUILD
    COMMAND 
        glslc ${SHADER_FILE} -o ${SHADER_BYTECODE_DIRECTORY}/${SHADER_FILE}.spv
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    )
endforeach()

# ---- custom target: hlslGenerate ----
make_directory(${SHADER_HLSL_GENERATED_DIR})
make_directory(${SHADER_HLSL_GENERATED_DIR}/glsl)
add_custom_target(hlslGenerate 
DEPENDS ${SHADER_SOURCES}
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders
)

foreach(SHADER_FILE ${SHADER_SOURCES})
    add_custom_command(TARGET hlslGenerate POST_BUILD
    COMMAND 
        ${SPIRV_CROSS_BIN} ${SHADER_BYTECODE_DIRECTORY}/${SHADER_FILE}.spv --output ${SHADER_HLSL_GENERATED_DIR}/${SHADER_FILE}.hlsl --hlsl --shader-model 40
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    )
endforeach()

# -------------------------------------------------- HLSL ---------------------------------------------------------------------------
set(HLSL_SHADER_VERT_SOURCES
    ./hlsl/PBRVert.hlsl
)
set(HLSL_SHADER_FRAG_SOURCES
    ./hlsl/PBRFrag.hlsl
)

make_directory(${SHADER_BYTECODE_DIRECTORY}/hlsl)
add_custom_target(hlslCompile 
    DEPENDS ${HLSL_SHADER_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# compile vertex shaders
foreach(SHADER_FILE ${HLSL_SHADER_VERT_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WLE)

    add_custom_command(TARGET hlslCompile
    COMMAND
        ${DXC_EXE_PATH} 
            ${SHADER_FILE} 
            -spirv 
            -Zi 
            -T vs_6_0 
            -E main 
            -Fo "${SHADER_BYTECODE_DIRECTORY}/hlsl/${SHADER_NAME}Debug.spv"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    ) 
endforeach()

# compile pixel shaders
foreach(SHADER_FILE ${HLSL_SHADER_FRAG_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WLE)

    add_custom_command(TARGET hlslCompile
    COMMAND
        ${DXC_EXE_PATH} 
            ${SHADER_FILE} 
            -spirv 
            -Zi 
            -T ps_6_0 
            -E main 
            -Fo "${SHADER_BYTECODE_DIRECTORY}/hlsl/${SHADER_NAME}Debug.spv"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    ) 
endforeach()

