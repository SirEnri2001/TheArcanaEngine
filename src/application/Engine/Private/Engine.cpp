#define COREGEOMETRY_INCLUDE
#define CORESCENE_INCLUDE
#define RENDERER_INCLUDE
#include <chrono>
#include <fstream>

#include "CoreMath.h"
#include "CoreMath.inl"
#include "CoreGeometry.h"
#include "CoreScene.h"
#include "CoreLog.inl"
#include "Renderer.h"


const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::string MODEL_PATH = "models/spot/spot_triangulated_good.obj";
const std::string TEXTURE_PATH = "models/spot/spot_texture.png";
const std::string MODEL2_PATH = "models/viking_room.obj";
const std::string TEXTURE2_PATH = "textures/viking_room.png";
const std::string VERT_SHADER_PATH = "shaderbytecode/glsl/BlinnPhong.vert";
const std::string FRAG_SHADER_PATH = "shaderbytecode/glsl/BlinnPhong.frag";

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}


struct UniformBufferObject {
    alignas(16) float4x4 model;
    alignas(16) float4x4 view;
    alignas(16) float4x4 proj;
    alignas(16) float4 viewPosition;
};

void updateUniformBuffer(UniformBufferObject& OutUniformBufferObject, float WindowHeight, float WindowWidth, glm::mat4 viewMat, float4 ViewPos) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    OutUniformBufferObject.model = glm::mat4(1.0f);//glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    OutUniformBufferObject.view = viewMat;
    OutUniformBufferObject.proj = glm::perspective(glm::radians(45.0f), WindowWidth / WindowHeight, 0.1f, 10.0f);
    OutUniformBufferObject.proj[1][1] *= -1;
    OutUniformBufferObject.viewPosition = ViewPos;
}
int main()
{
    //Log("Engine starts at ", "application mode", " ", 3);
    //Warning("This is a test warning. ");
    //Error("This is a test ERROR. ");
    Scene MainScene;
    std::string TestJson = R"({
    "Children":[
        {
            "Type":"Primitive",
            "Mesh":"cube.obj",
			"Transform":{
		        "Location":{
		            "x":1.0,
		            "y":2.0,
		            "z":3.0
		        },
		        "Rotation":{
		            "x":0.0,
		            "y":20.0,
		            "z":30.0
		        },
		        "Scale":{
		            "x":1.0,
		            "y":2.0,
		            "z":1.0
		        }
		    }
        }
    ]
})";
    Scene::LoadSceneJson(MainScene, TestJson);
    Mesh StaticMesh = Mesh::LoadObj(MODEL_PATH);
    Mesh StaticMesh2 = Mesh::LoadObj(MODEL2_PATH);
    StaticMesh.TexturePath = TEXTURE_PATH;
    StaticMesh2.TexturePath = TEXTURE2_PATH;
    std::vector<char> VertexShaderSPIRV = readFile(VERT_SHADER_PATH);
    std::vector<char> FragmentShaderSPIRV = readFile(FRAG_SHADER_PATH);

    // Renderer
    RendererContext::Get()->Initialize(WIDTH, HEIGHT);
    Renderer RendererInstance;
    MeshRenderProxy MeshProxy;
    MeshRenderProxy MeshProxy2;
    MeshProxy.Initialize(RendererContext::Get(), StaticMesh);
    MeshProxy2.Initialize(RendererContext::Get(), StaticMesh2);
    UniformBufferObject ubo;
    RHIUniform Uniform;
    Uniform.Initialize(&RendererContext::Get()->Context, sizeof(UniformBufferObject));
    RendererInstance.SetUniform(&Uniform, 0);
    RendererInstance.SetTextureSampler(&MeshProxy.Texture, 1);
    RendererInstance.Initialize(RendererContext::Get(), VertexShaderSPIRV, FragmentShaderSPIRV);
    RendererInstance.DrawScene(RendererContext::Get(), MeshProxy);
    RendererInstance.DrawScene(RendererContext::Get(), MeshProxy2);

    auto viewMat = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    float4 ViewPos = { 2.,2.,2.,1. };
    while (RendererContext::Get()->IsWindowAlive())
    {
        updateUniformBuffer(ubo, RendererContext::Get()->WindowManager.GetWindowHeight(),  RendererContext::Get()->WindowManager.GetWindowWidth(), viewMat, ViewPos);
        Uniform.CopyToBuffer(&RendererContext::Get()->Context, &ubo, sizeof(ubo));
        RendererInstance.UpdateFrame(RendererContext::Get());
    }
	return 0;
}
