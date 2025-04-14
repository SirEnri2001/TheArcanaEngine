cmake_minimum_required(VERSION 3.20)

# these shader files are relative paths to ./shader folder
set(SHADER_SOURCES
./glsl/BlinnPhong.frag
./glsl/BlinnPhong.vert
./glsl/ScreenPost.frag
./glsl/ScreenPost.vert
)
set(SHADER_BYTECODE_DIRECTORY ${CMAKE_SOURCE_DIR}/shaderbytecode)
# ensure all folders
make_directory(${SHADER_BYTECODE_DIRECTORY}/glsl/)
add_custom_target(glslCompile 
DEPENDS ${SHADER_SOURCES}
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

foreach(SHADER_FILE ${SHADER_SOURCES})
    add_custom_command(TARGET glslCompile
    COMMAND 
        glslc ${SHADER_FILE} -o ${SHADER_BYTECODE_DIRECTORY}/${SHADER_FILE}
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
    DEPENDS ${HLSL_SHADER_VERT_SOURCES}
    DEPENDS ${HLSL_SHADER_FRAG_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# compile vertex shaders
foreach(SHADER_FILE ${HLSL_SHADER_VERT_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WLE)

    add_custom_command(TARGET hlslCompile
    COMMAND
        ${DXC_EXE_PATH} 
            -spirv 
            -fspv-extension=SPV_KHR_non_semantic_info
            -fspv-debug=vulkan-with-source
            -T vs_6_0 
            -E main 
            ${SHADER_FILE} -Fo "${SHADER_BYTECODE_DIRECTORY}/hlsl/${SHADER_NAME}Debug.spv"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    ) 
endforeach()

# compile pixel shaders
foreach(SHADER_FILE ${HLSL_SHADER_FRAG_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WLE)

    add_custom_command(TARGET hlslCompile
    COMMAND
        ${DXC_EXE_PATH} 
            -spirv 
            -fspv-extension=SPV_KHR_non_semantic_info
            -fspv-debug=vulkan-with-source
            -T ps_6_0 
            -E main 
            ${SHADER_FILE} -Fo "${SHADER_BYTECODE_DIRECTORY}/hlsl/${SHADER_NAME}Debug.spv"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    ) 
endforeach()