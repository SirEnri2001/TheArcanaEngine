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