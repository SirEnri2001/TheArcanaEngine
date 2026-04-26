#define COREGEOMETRY_INCLUDE
#define CORESCENE_INCLUDE
#define RENDERER_INCLUDE
#define SHADERCOMPILER_INCLUDE
#define RHI_INCLUDE

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
#include "BlinnPhongRenderer.h"
#include "BaseRenderer.h"
#include "Renderer.h"
#include "Engine.h"

 #define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

 RenderControl Engine::GControl;

void Engine::ProcessInput(ImGuiSharedGlobals* ImGlobals)
{
    if (ImGlobals) {
        ImGui::SetCurrentContext(ImGlobals->Context);
        ImGui::SetAllocatorFunctions(ImGlobals->MemAllocFunc, ImGlobals->MemFreeFunc, ImGlobals->UserData);
    }
    ImGuiIO& io = ImGui::GetIO();
    
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

void Engine::Initialize(int width, int height, ERendererSelection Renderer, RHIBackend Backend, int InMaxFrames, const std::string& InOutputPath, bool bEnableValidation) {
    RendererSelection = Renderer;
    BackendSelection = Backend;
    MaxFrames = InMaxFrames;
    OutputPath = InOutputPath;

    if (RendererSelection == ERendererSelection::PathTracing) {
        PTRenderer.CreateRenderer(height, width, BackendSelection, bEnableValidation, Engine::ProcessInput);
        PTRenderer.CreateResource();
        CameraTransformLocalToWorld[0] = float4(0.f, 1.f, 0.f, 0.f);
        CameraTransformLocalToWorld[1] = float4(-1.f, 0.f, 0.f, 0.f);
        CameraTransformLocalToWorld[3] = float4(-0.27f, -0.8f, 0.27f, 1.f);
        GControl.CameraTransformLocalToWorld = CameraTransformLocalToWorld;

        PTRenderer.PTResource.SceneUniform->Initialize(PTRenderer.Env.Context, sizeof(ModelUniformObject), 8, RHIResourceState::BUFFER_UNIFORM);
        
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
        PTRenderer.PTResource.SceneUniform->CopyToBuffer(PTRenderer.Env.Context, Objects.data(), (uint32_t)(Objects.size() * sizeof(ModelUniformObject)));
        PTRenderer.PTResource.PrimitiveBuffer->Initialize(PTRenderer.Env.Context, (uint32_t)(sizeof(ModelUniformObject) * Objects.size()), RHIResourceState::BUFFER_SHADER_STORAGE);
        PTRenderer.PTResource.PrimitiveBuffer->CopyToBuffer(PTRenderer.Env.Context, Objects.data(), (uint32_t)(Objects.size() * sizeof(ModelUniformObject)));
    }
    else if (RendererSelection == ERendererSelection::BlinnPhong) {
        BPRenderer.CreateRenderer(height, width, BackendSelection, bEnableValidation, Engine::ProcessInput);
        BPRenderer.CreateResource();
        CameraTransformLocalToWorld = glm::translate(float4x4(1.0f), float3(-10.f, 0, 0.0f));
        GControl.CameraTransformLocalToWorld = CameraTransformLocalToWorld;
    }
    else if (RendererSelection == ERendererSelection::Base) {
        BRenderer.CreateRenderer(height, width, BackendSelection, bEnableValidation, Engine::ProcessInput);
    }
    else if (RendererSelection == ERendererSelection::Switch) {
        SRenderer.CreateRenderer(height, width, BackendSelection, bEnableValidation, Engine::ProcessInput);
        
        PTRenderer.SetEnv(SRenderer.Env);
        PTRenderer.CreateResource();

        BRenderer.SetEnv(SRenderer.Env);

        BPRenderer.SetEnv(SRenderer.Env);
        BPRenderer.CreateResource();

        CameraTransformLocalToWorld[0] = float4(0.f, 1.f, 0.f, 0.f);
        CameraTransformLocalToWorld[1] = float4(-1.f, 0.f, 0.f, 0.f);
        CameraTransformLocalToWorld[3] = float4(-0.27f, -0.8f, 0.27f, 1.f);
        GControl.CameraTransformLocalToWorld = CameraTransformLocalToWorld;

        PTRenderer.PTResource.SceneUniform->Initialize(PTRenderer.Env.Context, sizeof(ModelUniformObject), 8, RHIResourceState::BUFFER_UNIFORM);
        
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
        
        // Cow
        SetSceneUniform(0, Objects, glm::translate(float4x4(1.0), float3(-0.25f, 0.25f, 0.17f)) * 
            glm::rotate(float4x4(1.0), glm::radians(180.f+45.f), float3(0, 0, 1)) * 
            glm::rotate(float4x4(1.0), glm::radians(90.f), float3(1, 0, 0)) * 
            glm::scale(float4x4(1.0f), float3(0.15f, 0.15f, 0.15f)), float3(1.f, 1.f, 0.f), float3(0., 0., 0.));
        PTRenderer.cuo.modelUniformCount = Objects.size();
        PTRenderer.PTResource.SceneUniform->CopyToBuffer(PTRenderer.Env.Context, Objects.data(), (uint32_t)(Objects.size() * sizeof(ModelUniformObject)));
        PTRenderer.PTResource.PrimitiveBuffer->Initialize(PTRenderer.Env.Context, (uint32_t)(sizeof(ModelUniformObject) * Objects.size()), RHIResourceState::BUFFER_SHADER_STORAGE);
        PTRenderer.PTResource.PrimitiveBuffer->CopyToBuffer(PTRenderer.Env.Context, Objects.data(), (uint32_t)(Objects.size() * sizeof(ModelUniformObject)));

        SRenderer.AddRenderer(&PTRenderer, "Path Trace");
        SRenderer.AddRenderer(&BRenderer, "Base");
        SRenderer.AddRenderer(&BPRenderer, "Blinn Phong");
        SRenderer.CurrentSelection = 0;
    }
}


 void Engine::Run() {
    static auto startTime = std::chrono::high_resolution_clock::now();

    IRHIContext* ActiveContext = nullptr;
    IRenderer* ActiveRenderer = nullptr;
    if (RendererSelection == ERendererSelection::PathTracing) {
        ActiveContext = PTRenderer.Env.Context;
        ActiveRenderer = &PTRenderer;
    } else if (RendererSelection == ERendererSelection::BlinnPhong) {
        ActiveContext = BPRenderer.Env.Context;
        ActiveRenderer = &BPRenderer;
    } else if (RendererSelection == ERendererSelection::Base) {
        ActiveContext = BRenderer.Env.Context;
        ActiveRenderer = &BRenderer;
    } else if (RendererSelection == ERendererSelection::Switch) {
        ActiveContext = SRenderer.Env.Context;
        ActiveRenderer = &SRenderer;
    }

    if (!ActiveContext) return;

    int currentFrame = 0;
    while (ActiveContext->IsWindowAlive())
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        RendererEnvironment Env;
        ActiveRenderer->GetEnv(Env);
        ProcessInput(Env.ImGUI ? Env.ImGUI->GetGlobals() : nullptr);

        if (RendererSelection == ERendererSelection::PathTracing) {
            PTRenderer.UpdateUniformBuffer(GControl.CameraTransformLocalToWorld, time, &GControl);
            PTRenderer.Render(float4(1.f), &GControl);
            PTRenderer.ProcessInput();
        } else if (RendererSelection == ERendererSelection::BlinnPhong) {
            BPRenderer.Render(GControl.CameraTransformLocalToWorld[3], &GControl);
            BPRenderer.Env.Context->ProcessFrameInput();
        } else if (RendererSelection == ERendererSelection::Base) {
            BRenderer.Render(float4(0.f), &GControl);
            BRenderer.Env.Context->ProcessFrameInput();
        } else if (RendererSelection == ERendererSelection::Switch) {
            if (SRenderer.CurrentSelection >= 0 && SRenderer.CurrentSelection < SRenderer.AddedRenderers.size()) {
                if (SRenderer.AddedRenderers[SRenderer.CurrentSelection] == static_cast<BaseRenderer*>(&PTRenderer)) {
                    PTRenderer.UpdateUniformBuffer(GControl.CameraTransformLocalToWorld, time, &GControl);
                    PTRenderer.ProcessInput();
                }
            }
            SRenderer.Render(float4(1.f), &GControl);
            SRenderer.Env.Context->ProcessFrameInput();
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
    ERendererSelection renderer = ERendererSelection::Switch;
    RHIBackend rhi = RHIBackend::Vulkan;
    int maxFrames = -1;
    std::string outputPath = "";
    bool enableValidation = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--renderer" && i + 1 < argc) {
            std::string val = argv[++i];
            if (val == "PathTracing") renderer = ERendererSelection::PathTracing;
            else if (val == "BlinnPhong") renderer = ERendererSelection::BlinnPhong;
            else if (val == "Base") renderer = ERendererSelection::Base;
            else if (val == "Switch") renderer = ERendererSelection::Switch;
        } else if (arg == "--rhi" && i + 1 < argc) {
            std::string val = argv[++i];
            if (val == "D3D12") rhi = RHIBackend::D3D12;
            else if (val == "Vulkan") rhi = RHIBackend::Vulkan;
        } else if (arg == "--frame-count" && i + 1 < argc) {
            maxFrames = std::stoi(argv[++i]);
        } else if (arg == "--output" && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (arg == "-nsprofile") {
            enableValidation = false;
        }
    }

    Engine engine;
    engine.Initialize(1000, 1000, renderer, rhi, maxFrames, outputPath, enableValidation);
    engine.Run();
    engine.Shutdown();
    return 0;
}
#elif 0

#include "spirv_glsl.hpp"
#include <vector>
#include <utility>

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