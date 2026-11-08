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

add_subdirectory(./tinygltf-3.0.0)
set_target_properties(tinygltf PROPERTIES FOLDER "Thirdparties/tinygltf-3.0.0")

add_subdirectory(./DirectX-Headers-1.615.0)
set(DirectXHeaders_INCLUDE_DIRS ${ThirdPartiesDir}/DirectX-Headers-1.615.0/include)
set_target_properties(DirectX-Headers PROPERTIES FOLDER "Thirdparties/DirectX-Headers-1.615.0")

# DXC EXE
# Resolution order: user override (-DDXC_EXE_PATH=...) -> vendored build
# downloaded by DXC_downloader.bat -> system dxc (the Vulkan SDK ships one
# with SPIR-V codegen enabled). Stored in CACHE so shaders/CMakeLists.txt,
# which is included from the top-level scope, can see it.
if(NOT DXC_EXE_PATH)
    file(GLOB _dxc_exe_path "${CMAKE_SOURCE_DIR}/src/thirdparties/dxc_2025_02_20/bin/x64/dxc.exe")
    if(_dxc_exe_path)
        list(GET _dxc_exe_path 0 _dxc_resolved)
    else()
        find_program(_dxc_system_exe NAMES dxc dxc.exe
            HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VK_SDK_PATH}/Bin")
        set(_dxc_resolved "${_dxc_system_exe}")
    endif()

    if(_dxc_resolved)
        set(DXC_EXE_PATH "${_dxc_resolved}" CACHE FILEPATH
            "Path to DirectX Shader Compiler with SPIRV CodeGen enabled" FORCE)
        message(STATUS "DXC: ${DXC_EXE_PATH}")
    else()
        message(WARNING "DXC not found! Set -DDXC_EXE_PATH=<path> or run the batch script under ./src/thirdparties/dxc_2025_02_20 to download one. HLSL shader targets will be skipped.")
    endif()
endif()

add_subdirectory(./SPIRV-Cross-vulkan-sdk-1.4.335.0)
set_target_properties(spirv-cross spirv-cross-c spirv-cross-core spirv-cross-cpp 
spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl spirv-cross-reflect spirv-cross-util 
PROPERTIES FOLDER "Thirdparties/SPIRV-Cross-vulkan-sdk-1.4.335.0")
