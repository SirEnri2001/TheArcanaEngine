# specify third party dependencies
set(ThirdPartiesDir ./src/thirdparties)

add_subdirectory(${ThirdPartiesDir}/glfw-3.4)
set(glfw_INCLUDE_DIRS ${ThirdPartiesDir}/glfw-3.4/include)
set_target_properties(glfw update_mappings PROPERTIES FOLDER "Thirdparties/GLFW")

add_subdirectory(${ThirdPartiesDir}/glm-1.0.1)
set(glm_INCLUDE_DIRS ${ThirdPartiesDir}/glm-1.0.1)
set_target_properties(glm PROPERTIES FOLDER "Thirdparties/glm")

add_subdirectory(${ThirdPartiesDir}/imgui-1.91.8)
set(imgui_INCLUDE_DIRS ${ThirdPartiesDir}/imgui-1.91.8)
set_target_properties(imgui PROPERTIES FOLDER "Thirdparties/imgui-1.91.8")