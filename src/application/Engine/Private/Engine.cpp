#define COREGEOMETRY_INCLUDE
#define CORESCENE_INCLUDE
#define RENDERER_INCLUDE
#include <chrono>
#include <fstream>
#include <windows.h>

#include "CoreMath.h"
#include "CoreMath.inl"
#include "CoreGeometry.h"
#include "CoreScene.h"
#include "CoreLog.inl"
#include "Renderer.h"
#include "RHIImGuiHelper.h"


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
auto viewMat = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
float4 ViewPos = { 2.,2.,2.,1. };
void DrawUI(ImGuiSharedGlobals* ImGlobals)
{
    ImGui::SetCurrentContext(ImGlobals->Context);
    ImGui::SetAllocatorFunctions(ImGlobals->MemAllocFunc, ImGlobals->MemFreeFunc, ImGlobals->UserData);
    static bool show_demo_window = true;
    static bool show_another_window = true;
    static float4 clear_color;
    static ImGuiIO& io = ImGui::GetIO();
    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
    {
        ImGui::ShowDemoWindow(&show_demo_window);
    }

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::End();
    }

    // 3. Show another simple window.
    if (show_another_window)
    {
        ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        ImGui::Text("io.WantCaptureMouse = %d", io.WantCaptureMouse);
        if (ImGui::Button("Close Me"))
            show_another_window = false;
        ImGui::End();
    }
    if (!io.WantCaptureMouse && (io.MouseDown[0] || io.MouseDown[1])) // User operate with actual scene
    {
        float DeltaX = io.MousePos.x - io.MousePosPrev.x;
        float DeltaY = io.MousePos.y - io.MousePosPrev.y;

        viewMat = glm::rotate(glm::mat4(1.f), DeltaX * 0.01f, glm::vec3(0.0f, 1.0f, 0.0f)) * viewMat;
        viewMat = glm::rotate(glm::mat4(1.f), DeltaY * 0.01f, glm::vec3(1.0f, 0.0f, 0.0f)) * viewMat;
        float3 PlayerMove = { 0.f, 0.f, 0.f };
        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            PlayerMove += float3(0., 0., 1.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            PlayerMove += float3(0., 0., -1.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            PlayerMove += float3(1., 0., 0.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            PlayerMove += float3(-1., 0., 0.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q))
        {
            PlayerMove += float3(0., -1., 0.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_E))
        {
            PlayerMove += float3(0., 1., 0.);
        }
        viewMat = glm::translate(glm::mat4(1.0), PlayerMove * 0.1f) * viewMat;
    }
}


// int main()
// {
//     //Log("Engine starts at ", "application mode", " ", 3);
//     //Warning("This is a test warning. ");
//     //Error("This is a test ERROR. ");
//     Scene MainScene;
//     std::string TestJson = R"({
//     "Children":[
//         {
//             "Type":"Primitive",
//             "Mesh":"cube.obj",
// 			"Transform":{
// 		        "Location":{
// 		            "x":1.0,
// 		            "y":2.0,
// 		            "z":3.0
// 		        },
// 		        "Rotation":{
// 		            "x":0.0,
// 		            "y":20.0,
// 		            "z":30.0
// 		        },
// 		        "Scale":{
// 		            "x":1.0,
// 		            "y":2.0,
// 		            "z":1.0
// 		        }
// 		    }
//         }
//     ]
// })";
//     Scene::LoadSceneJson(MainScene, TestJson);
//     Mesh StaticMesh = Mesh::LoadObj(MODEL_PATH);
//     Mesh StaticMesh2 = Mesh::LoadObj(MODEL2_PATH);
//     StaticMesh.TexturePath = TEXTURE_PATH;
//     StaticMesh2.TexturePath = TEXTURE2_PATH;
//     std::vector<char> VertexShaderSPIRV = readFile(VERT_SHADER_PATH);
//     std::vector<char> FragmentShaderSPIRV = readFile(FRAG_SHADER_PATH);

//     std::vector<char> postprocessvertSPV = readFile("shaderbytecode/glsl/ScreenPost.vert");
//     std::vector<char> postprocessfragSPV = readFile("shaderbytecode/glsl/ScreenPost.frag");

//     // Renderer
//     RendererContext::Get()->Initialize(WIDTH, HEIGHT);
//     Renderer RendererInstance;
//     RendererInstance.pFuncImDraw = DrawUI;
//     MeshRenderProxy MeshProxy;
//     MeshRenderProxy MeshProxy2;
//     MeshProxy.Initialize(RendererContext::Get(), StaticMesh);
//     MeshProxy2.Initialize(RendererContext::Get(), StaticMesh2);
//     UniformBufferObject ubo;
//     RHIUniform Uniform;
//     Uniform.Initialize(&RendererContext::Get()->Context, sizeof(UniformBufferObject));
//     RendererInstance.SetUniform(&Uniform, 0);
//     RendererInstance.SetTextureSampler(&MeshProxy.Texture, 1);
//     RendererInstance.Initialize(RendererContext::Get(), VertexShaderSPIRV, FragmentShaderSPIRV, postprocessvertSPV, postprocessfragSPV);
//     RendererInstance.DrawScene(RendererContext::Get(), MeshProxy);
//     RendererInstance.DrawScene(RendererContext::Get(), MeshProxy2);
//     while (RendererContext::Get()->IsWindowAlive())
//     {
//         updateUniformBuffer(ubo, RendererContext::Get()->WindowManager.GetWindowHeight(),  RendererContext::Get()->WindowManager.GetWindowWidth(), viewMat, ViewPos);
//         Uniform.CopyToBuffer(&RendererContext::Get()->Context, &ubo, sizeof(ubo));
//         RendererInstance.UpdateFrame(RendererContext::Get());
//     }
// 	return 0;
// }

int main()
{
	GRHIImplementationSelection = RHIImplement_D3D12;
    RHIPlatformSupport::Get()->Initialize();
    RHIWindowManager Manager;
    Manager.Initialize(RHIPlatformSupport::Get(), 1280, 720);
    RHIContext Context;
    Context.Initialize(RHIPlatformSupport::Get());
    Manager.InitializeSwapchain(&Context, RHIPlatformSupport::Get());
    RHIPipelineFactory PipelineFactory;
    RHIPipelineObject PipelineObject;
    PipelineFactory.InitializePipelineObject(&PipelineObject, &Context, (RHIRenderPass*)nullptr);
    RHIGraphicsDispatcher GraphicsDispatcher;
    GraphicsDispatcher.Initialize(&Context);
    // Main sample loop.
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // Process any messages in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        GraphicsDispatcher.Dispatch(&Manager, &PipelineObject, 1, 0, 1);
        GraphicsDispatcher.EndPresentPassAndSubmit(&Context, &Manager);
    }



    //// Return this part of the WM_QUIT message to Windows.
    //return static_cast<char>(msg.wParam);
}