#define COREGEOMETRY_INCLUDE
#define CORESCENE_INCLUDE
#define RENDERER_INCLUDE
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
const std::string VERT_SHADER_PATH = "shaders/vert.spv";
const std::string FRAG_SHADER_PATH = "shaders/frag.spv";

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


int main()
{
    Log("Engine starts at ", "application mode", " ", 3);
    Warning("This is a test warning. ");
    Error("This is a test ERROR. ");
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
    StaticMesh.TexturePath = TEXTURE_PATH;
    std::vector<char> VertexShaderSPIRV = readFile(VERT_SHADER_PATH);
    std::vector<char> FragmentShaderSPIRV = readFile(FRAG_SHADER_PATH);

    // Renderer
    RendererContext::Get()->Initialize(WIDTH, HEIGHT);
    Renderer RendererInstance;
    MeshRenderProxy MeshProxy;
    MeshProxy.Initialize(RendererContext::Get(), StaticMesh);
    RendererInstance.Initialize(RendererContext::Get());
    RendererInstance.DrawScene(RendererContext::Get(), MeshProxy, VertexShaderSPIRV, FragmentShaderSPIRV);
    RendererInstance.DrawUI();
    while (RendererContext::Get()->IsWindowAlive())
    {
        RendererInstance.UpdateFrame(RendererContext::Get());
    }
	return 0;
}
