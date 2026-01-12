add_subdirectory(./glfw-3.4)
set_target_properties(glfw update_mappings PROPERTIES FOLDER "Thirdparties/GLFW")

add_subdirectory(./glm-1.0.1)
set_target_properties(glm PROPERTIES FOLDER "Thirdparties/glm")

add_subdirectory(./imgui-1.91.8)
set_target_properties(imgui PROPERTIES FOLDER "Thirdparties/imgui-1.91.8")

add_subdirectory(./nlohmann_json-3.11.3)
set_target_properties(nlohmann_json PROPERTIES FOLDER "Thirdparties/nlohmann_json-3.11.3")

add_subdirectory(./stb_image-2.30)
set_target_properties(stb_image PROPERTIES FOLDER "Thirdparties/stb_image-2.30")

add_subdirectory(./tinyobjloader-1.0.6)
set_target_properties(tiny_obj_loader PROPERTIES FOLDER "Thirdparties/tiny_obj_loader-1.06")

add_subdirectory(./DirectX-Headers-1.615.0)
set(DirectXHeaders_INCLUDE_DIRS ${ThirdPartiesDir}/DirectX-Headers-1.615.0/include)
set_target_properties(DirectX-Headers PROPERTIES FOLDER "Thirdparties/DirectX-Headers-1.615.0")

# DXC EXE
file(GLOB _dxc_path "${CMAKE_SOURCE_DIR}/src/thirdparties/dxc_2025_02_20")
file(GLOB _dxc_exe_path "${_dxc_path}/bin/x64/dxc.exe")

if(_dxc_exe_path)
    list(GET _dxc_exe_path 0 _first_match)
    set(DXC_EXE_PATH "${_first_match}" CACHE FILEPATH "Path to DirectX Shader Compiler with SPIRV CodeGen enabled")
else()
    message(WARNING "DXC not found! Need to manually set DXC_EXE_PATH or run the batch script under ./src/thirdparties/dxc_2025_02_20 to download one.")
    set(DXC_EXE_PATH "" CACHE FILEPATH "Path to DirectX Shader Compiler with SPIRV CodeGen enabled")
endif()

add_subdirectory(./SPIRV-Cross-vulkan-sdk-1.4.335.0)
set_target_properties(spirv-cross spirv-cross-c spirv-cross-core spirv-cross-cpp 
spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl spirv-cross-reflect spirv-cross-util 
PROPERTIES FOLDER "Thirdparties/SPIRV-Cross-vulkan-sdk-1.4.335.0")
