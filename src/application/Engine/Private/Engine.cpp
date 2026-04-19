#define COREGEOMETRY_INCLUDE
#define CORESCENE_INCLUDE
#define RENDERER_INCLUDE
#define SHADERCOMPILER_INCLUDE
#define PBR_RENDERER_INCLUDE
#define RHI_INCLUDE
#define PATHTRACERENDERER_INCLUDE

#include <chrono>
#include <fstream>

#include "CoreMath.h"
#include "CoreMath.inl"
#include "CoreGeometry.h"
#include "CoreScene.h"
#include "CoreLog.inl"
#include "RHI.h"
#include "RHIImGuiHelper.h"
#include "PathTraceRenderer.h"
#include "Renderer.h"
#include "Engine.h"

 #define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

const std::string MODEL_PATH = "models/spot/spot_triangulated_good.obj";
const std::string TEXTURE_PATH = "models/spot/spot_texture.png";
const std::string MODEL2_PATH = "models/viking_room.obj";
const std::string TEXTURE2_PATH = "textures/viking_room.png";
const std::string VERT_SHADER_PATH = "shaderbytecode/glsl/BlinnPhong.vert";
const std::string FRAG_SHADER_PATH = "shaderbytecode/glsl/BlinnPhong.frag";
const std::string CUBE_PATH = "models/cube/cube.obj";

 RenderControl Engine::GControl;

void readFile_U32I(const std::string& filename, std::vector<uint32_t>& buffer_u32) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    buffer_u32.resize(buffer.size()/4);
    for (int i = 0; i < buffer.size(); i+=4) {
        // Assuming little-endian file format; adjust if needed
        buffer_u32[i / 4] = static_cast<uint32_t>(buffer[i]) |
            (static_cast<uint32_t>(buffer[i + 1]) << 8) |
            (static_cast<uint32_t>(buffer[i + 2]) << 16) |
            (static_cast<uint32_t>(buffer[i + 3]) << 24);
    }
}

template<class T>
std::vector<T> read_spirv_file(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "Failed to open SPIR-V file: %s\n", path);
        return {};
    }

    fseek(file, 0, SEEK_END);
    long len = ftell(file) / sizeof(T);
    rewind(file);

    std::vector<T> spirv(len);
    if (fread(spirv.data(), sizeof(T), len, file) != size_t(len))
        spirv.clear();

    fclose(file);
    return spirv;
}

void Engine::DrawUI(ImGuiSharedGlobals* ImGlobals)
{
    ImGui::SetCurrentContext(ImGlobals->Context);
    ImGui::SetAllocatorFunctions(ImGlobals->MemAllocFunc, ImGlobals->MemFreeFunc, ImGlobals->UserData);
    static bool show_demo_window = false;
    static bool show_another_window = false;
    static float4 clear_color;
    static ImGuiIO& io = ImGui::GetIO();
    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
    {
        ImGui::ShowDemoWindow(&show_demo_window);
    }

    // 2. Show a simple window that we create ourselves. W e use a Begin/End pair to create a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Toolbar");                          // Create a window called "Hello, world!" and append into it.
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

        if (ImGui::Button("Recompile Shaders")) {
            Engine::GControl.bClear = true;
            Engine::GControl.Recompile = true;
            Engine::GControl.TestShaders = false;
        }

        if (ImGui::Button("Pause")) {
            Engine::GControl.RenderingPaused = true;
            ImGui::OpenPopup("Pause Rendering");
        }

        if (ImGui::Button("Clear")) {
            Engine::GControl.bClear = true;
        }

        if (ImGui::SliderInt("MaxFrames",&Engine::GControl.maxFrames, -1, 5000, "%d")) {
        }

        ImGui::SliderFloat("Moving speed", &Engine::GControl.movingSpeed, 0.1f, 1.f);

        ImGui::Checkbox("Enable Gamma", &Engine::GControl.enableGamma);

        ImGui::Text("Frame: %d", Engine::GControl.currentFrame);
        ImGui::Text("FPS: %.1f", io.Framerate);
        
        ImGui::Separator();

        if (Engine::GControl.ShaderCompileHasError) {
            ImGui::OpenPopup("Shader Compiler Error");
        }

        // Always center this window when appearing
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Pause Rendering", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Rendering paused. ");
            ImGui::Separator();

            if (ImGui::Button("Continue", ImVec2(120, 0)))
            {
                Engine::GControl.RenderingPaused = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Shader Compiler Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Shader has compilation error, check console for more information. ");
            ImGui::Separator();

            if (ImGui::Button("Retry", ImVec2(120, 0))) 
            { 
                //CompileAllShaders(&Engine::GControl);
                ImGui::CloseCurrentPopup(); 
            }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }

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
        Engine::GControl.isClick = true;
        float DeltaX = io.MousePos.x - io.MousePosPrev.x;
        float DeltaY = io.MousePos.y - io.MousePosPrev.y;

        DeltaX *= 0.001;
        DeltaY *= 0.001;

        float3 PlayerMove = { 0.f, 0.f, 0.f };
        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            PlayerMove += float3(1.f, 0.f, 0.f);
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            PlayerMove -= float3(1.f, 0.f, 0.f);
        }
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            PlayerMove += float3(0.f, 1.f, 0.f);
        }
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            PlayerMove -= float3(0.f, 1.f, 0.f);
        }
        if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
        {
            PlayerMove += float3(0.f, 0.f, 1.f);
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_Space))
        {
            PlayerMove -= float3(0.f, 0.f, 1.f);
        }
        float4x4 ViewTransform = float4x4(1.0f);
        float4x4 PitchRotate = glm::rotate(float4x4(1.0f), -DeltaY, float3(0.f, 1.f, 0.f));
        float4x4 YawRotate = glm::rotate(float4x4(1.0f), DeltaX, float3(0.f, 0.f, 1.f));
        float4x4 Translate = glm::translate(float4x4(1.0f), PlayerMove * 0.01f * Engine::GControl.movingSpeed);
        Engine::GControl.CameraTransformLocalToWorld = Engine::GControl.CameraTransformLocalToWorld * Translate * PitchRotate * YawRotate;
    }else
    {
        Engine::GControl.isClick = false;
    }
}

struct ModelUniformObject {
    int modelType;
    alignas(16) float4x4 model;
    alignas(16) float4x4 modelInv;
    alignas(16) float4x4 modelInvTranspose;
    alignas(16) float3 color;
    alignas(16) float3 emission;
};

void SetSceneUniform(int modelType, std::vector<ModelUniformObject>& Objects, float4x4 ModelMat, float3 Color, float3 emission) {
    ModelUniformObject uniformobject;
    uniformobject.modelType = modelType;
    uniformobject.model = ModelMat;
    uniformobject.modelInv = glm::inverse(ModelMat);
    uniformobject.modelInvTranspose = glm::transpose(uniformobject.modelInv);
    uniformobject.color = Color;
    uniformobject.emission = emission;
    Objects.push_back(uniformobject);
}

#if 1
Engine::Engine() {
    CameraTransformLocalToWorld = float4x4(1.f);
}
Engine::~Engine() {}

 void Engine::Initialize(int width, int height, ERendererSelection Renderer, RHIBackend Backend, int InMaxFrames, const std::string& InOutputPath) {
    RendererSelection = Renderer;
    BackendSelection = Backend;
    MaxFrames = InMaxFrames;
    OutputPath = InOutputPath;

    if (RendererSelection == ERendererSelection::PathTracing) {
        PTRenderer.pFuncImDraw = &Engine::DrawUI;
        PTRenderer.CreateRenderer(width, height, BackendSelection);

        CameraTransformLocalToWorld[0] = float4(0.f, 1.f, 0.f, 0.f);
        CameraTransformLocalToWorld[1] = float4(-1.f, 0.f, 0.f, 0.f);
        CameraTransformLocalToWorld[3] = float4(-0.27f, -0.8f, 0.27f, 1.f);
        GControl.CameraTransformLocalToWorld = CameraTransformLocalToWorld;

        PTRenderer.SceneUniform->Initialize(PTRenderer.Context, sizeof(ModelUniformObject), 8, RHIResourceState::BUFFER_UNIFORM);

        std::vector<ModelUniformObject> Objects;
        
        // Light
        SetSceneUniform(2, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.535f)) * glm::rotate(float4x4(1.0), glm::radians(180.f), float3(0, 1, 0.)) * glm::scale(float4x4(1.0f), float3(0.065f, 0.05f, 1.f) * 2.f), float3(1.f, 1.f, 1.f), float3(1., 1., 1.));

        // Floor
        SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.27f, 0.001f)), float3(0.8f, 0.8f, 0.8f), float3(0., 0., 0.));
        // Celling
        SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.54f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.27f, 0.001f)), float3(0.8f, 0.8f, 0.8f), float3(0., 0., 0.));
        // Leftwall
        SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(-0.54f, 0.27f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.001f, 0.27f, 0.27f)), float3(1.f, 0.f, 0.f), float3(0., 0., 0.));
        // Rightwall
        SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(0.f, 0.27f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.001f, 0.27f, 0.27f)), float3(0.f, 1.f, 0.f), float3(0., 0., 0.));
        // Backwall
        SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.54f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.001f, 0.27f)), float3(0.8f, 0.8f, 0.8f), float3(0., 0., 0.));
        //// Shortbox
        //SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(-0.185f, 0.169f, 0.0825f)) * glm::rotate(float4x4(1.0), glm::radians(-196.62f), float3(0, 0, 1.)) * glm::scale(float4x4(1.0f), float3(0.085f, 0.085f, 0.085f)), float3(0.8f, 0.8f, 0.8f), float3(0., 0., 0.));
        //// Tallbox
        //SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(-0.368f, 0.351f, 0.165f)) * glm::rotate(float4x4(1.0), glm::radians(-252.77f), float3(0, 0, 1.)) * glm::scale(float4x4(1.0f), float3(0.085f, 0.085f, 0.17f)), float3(0.8f, 0.8f, 0.8f), float3(0., 0., 0.));
        
        // Cow
        SetSceneUniform(0, Objects, glm::translate(float4x4(1.0), float3(-0.25f, 0.25f, 0.17f)) * 
            glm::rotate(float4x4(1.0), glm::radians(180.f+45.f), float3(0, 0, 1)) * 
            glm::rotate(float4x4(1.0), glm::radians(90.f), float3(1, 0, 0)) * 
            glm::scale(float4x4(1.0f), float3(0.15f, 0.15f, 0.15f)), float3(1.f, 1.f, 0.f), float3(0., 0., 0.));
        PTRenderer.cuo.modelUniformCount = Objects.size();
        PTRenderer.SceneUniform->CopyToBuffer(PTRenderer.Context, Objects.data(), (uint32_t)(Objects.size() * sizeof(ModelUniformObject)));
        PTRenderer.PrimitiveBuffer->Initialize(PTRenderer.Context, (uint32_t)(sizeof(ModelUniformObject) * Objects.size()), RHIResourceState::BUFFER_SHADER_STORAGE);
        PTRenderer.PrimitiveBuffer->CopyToBuffer(PTRenderer.Context, Objects.data(), (uint32_t)(Objects.size() * sizeof(ModelUniformObject)));
    }
    else if (RendererSelection == ERendererSelection::BlinnPhong) {
        BPRenderer.pFuncImDraw = &Engine::DrawUI;
        BPRenderer.CreateRenderer(width, height, BackendSelection);
        
        CameraTransformLocalToWorld = glm::translate(float4x4(1.0f), float3(-10.f, 0, 0.0f));
        GControl.CameraTransformLocalToWorld = CameraTransformLocalToWorld;
    }
}


 void Engine::Run() {
    static auto startTime = std::chrono::high_resolution_clock::now();

    IRHIContext* ActiveContext = nullptr;
    IRenderer* ActiveRenderer = nullptr;
    if (RendererSelection == ERendererSelection::PathTracing) {
        ActiveContext = PTRenderer.Context;
        ActiveRenderer = &PTRenderer;
    } else if (RendererSelection == ERendererSelection::BlinnPhong) {
        ActiveContext = BPRenderer.Context;
        ActiveRenderer = &BPRenderer;
    }

    if (!ActiveContext) return;

    int currentFrame = 0;
    while (ActiveContext->IsWindowAlive())
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        if (RendererSelection == ERendererSelection::PathTracing) {
            PTRenderer.UpdateUniformBuffer(GControl.CameraTransformLocalToWorld, time, &GControl);
            PTRenderer.Render(float4(1.f), &GControl);
            PTRenderer.ProcessInput();
        } else if (RendererSelection == ERendererSelection::BlinnPhong) {
            BPRenderer.Render(GControl.CameraTransformLocalToWorld[3], &GControl);
            BPRenderer.Context->ProcessFrameInput();
        }

        currentFrame++;
        if (MaxFrames > 0 && currentFrame >= MaxFrames) {
            if (!OutputPath.empty()) {
                ActiveRenderer->CaptureFrame(OutputPath);
            }
            break;
        }
    }
}


void Engine::Shutdown() {}

 int main(int argc, char** argv) {
    ERendererSelection renderer = ERendererSelection::PathTracing;
    RHIBackend rhi = RHIBackend::Vulkan;
    int maxFrames = -1;
    std::string outputPath = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--renderer" && i + 1 < argc) {
            std::string val = argv[++i];
            if (val == "PathTracing") renderer = ERendererSelection::PathTracing;
            else if (val == "BlinnPhong") renderer = ERendererSelection::BlinnPhong;
        } else if (arg == "--rhi" && i + 1 < argc) {
            std::string val = argv[++i];
            if (val == "D3D12") rhi = RHIBackend::D3D12;
            else if (val == "Vulkan") rhi = RHIBackend::Vulkan;
        } else if (arg == "--frame-count" && i + 1 < argc) {
            maxFrames = std::stoi(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            outputPath = argv[++i];
        }
    }

    Engine engine;
    engine.Initialize(1000, 1000, renderer, rhi, maxFrames, outputPath);
    engine.Run();
    engine.Shutdown();
    return 0;
}
#elif 0

#include "spirv_glsl.hpp"
#include <vector>
#include <utility>

int main()
{
    //readFile("shaderbytecode/glsl/PathTracer.frag");
    // Read SPIR-V from disk or similar.
    
    std::vector<char> spirv_binary = readFile("shaderbytecode/glsl/PathTracer.vert");
    //std::vector<char> spirv_binary = read_spirv_file<char>("shaderbytecode/glsl/PathTracer.vert");

    spirv_cross::CompilerGLSL glsl(reinterpret_cast<const uint32_t*>(spirv_binary.data()), spirv_binary.size()/4);

    // The SPIR-V is now parsed, and we can perform reflection on it.
    spirv_cross::ShaderResources resources = glsl.get_shader_resources();

    // Get all sampled images in the shader.
    for (auto& resource : resources.sampled_images)
    {
        unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
        unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
        printf("Image %s at set = %u, binding = %u\n", resource.name.c_str(), set, binding);

        // Modify the decoration to prepare it for GLSL.
        glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);

        // Some arbitrary remapping if we want.
        glsl.set_decoration(resource.id, spv::DecorationBinding, set * 16 + binding);
    }

    // Set some options.
    spirv_cross::CompilerGLSL::Options options;
    options.version = 310;
    options.es = true;
    glsl.set_common_options(options);

    // Compile to GLSL, ready to give to GL driver.
    std::string source = glsl.compile();
    std::cout << source;
}

int PBRRendererTest()
{
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
		            "y":0.0,
		            "z":0.0
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

    //RendererContext::Get()->Initialize(WIDTH, HEIGHT);

    //Scene::LoadSceneJson(MainScene, TestJson);
    //Mesh StaticMesh = Mesh::LoadObj(MODEL_PATH);
    //Mesh StaticMesh2 = Mesh::LoadObj(MODEL2_PATH);
    //StaticMesh.TexturePath = TEXTURE_PATH;

    //PBR::MaterialPropertyData roughBluePlastic{ {0.f, 0.f, 1.f}, 0.01f, 0.01f };
    //PBR::MaterialPropertyData shinyRedMetal{ {1.f, 0.f, 0.f}, 0.9f, 1.0f };

    //PBR::PBRMeshRenderProxy MeshProxy;
    //PBR::PBRMeshRenderProxy MeshProxy2;
    //MeshProxy.Initialize(RendererContext::Get(), StaticMesh, &shinyRedMetal);
    //MeshProxy2.Initialize(RendererContext::Get(), StaticMesh2, &roughBluePlastic);


    //// Renderer
    //std::vector<char> PBRVertSPV = readFile("shaderbytecode/hlsl/PBRVertDebug.spv");
    //std::vector<char> PBRPixelSPV = readFile("shaderbytecode/hlsl/PBRFragDebug.spv");
    //PBRRenderer RendererInstance;    RendererInstance.Initialize(RendererContext::Get(), PBRVertSPV, PBRPixelSPV);


    //// Draw
    //RendererInstance.AddSceneObject(MeshProxy);
    //RendererInstance.AddSceneObject(MeshProxy2);

    //UniformBufferObject ubo;
    //PBR::LightingData lightingData{ glm::normalize(float3(0, 0, 1)) };
    //PBR::TransformationData transformationData;
    //while (RendererContext::Get()->IsWindowAlive())
    //{
    //    // calculate camera
    //    updateUniformBuffer(ubo, RendererContext::Get()->WindowManager.GetWindowHeight(), RendererContext::Get()->WindowManager.GetWindowWidth(), viewMat, ViewPos);
    //    transformationData.model = ubo.model;
    //    transformationData.view = ubo.view;
    //    transformationData.viewPosition = ubo.viewPosition;
    //    transformationData.proj = ubo.proj;

    //    // update uniform buffer
    //    RendererInstance.SetUniform(PBR::RendererUniformType::Transformation, &transformationData);
    //    RendererInstance.SetUniform(PBR::RendererUniformType::Lighting, &lightingData);
    //    // RendererInstance.SetUniform(PBR::RendererUniformType::MaterialProperty, &roughBluePlastic);

    //    RendererInstance.UpdateFrame();
    //}
    return 0;
}
#endif