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

