#define COREGEOMETRY_INCLUDE
#define CORESCENE_INCLUDE
#define RENDERER_INCLUDE
#define SHADERCOMPILER_INCLUDE
#define RHI_INCLUDE

#include <algorithm>
#include <chrono>
#include <cmath>
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

    // Camera-local frame (see Initialize): +X forward, +Y left, +Z up.
    // The scene is Z-up. Yaw is applied about world +Z and pitch about the
    // camera's own +Y, so the horizon stays level and no roll accumulates.
    const float3 LocalForward(1.f, 0.f, 0.f);
    const float3 LocalLeft(0.f, 1.f, 0.f);
    const float3 LocalUp(0.f, 0.f, 1.f);
    const float3 WorldUp(0.f, 0.f, 1.f);
    constexpr float MaxPitch = 1.55334303f;   // 89 degrees, in radians
    constexpr float MaxDeltaTime = 0.1f;      // avoid a huge jump after a stall
    constexpr float MaxMouseDelta = 100.f;    // pixels per frame

    float4x4& Camera = GControl.CameraTransformLocalToWorld;
    float3 Position(Camera[3]);
    glm::quat Orientation = GControl.CameraOrientation;
    bool bCameraChanged = false;

    // ---- WASD / QE in player coordinates ----
    // Movement no longer requires a mouse button to be held down.
    float3 Move(0.f);
    if (!io.WantCaptureKeyboard)
    {
        if (ImGui::IsKeyDown(ImGuiKey_W)) Move += LocalForward;
        if (ImGui::IsKeyDown(ImGuiKey_S)) Move -= LocalForward;
        if (ImGui::IsKeyDown(ImGuiKey_A)) Move += LocalLeft;
        if (ImGui::IsKeyDown(ImGuiKey_D)) Move -= LocalLeft;
        if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) Move += LocalUp;
        if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_Space)) Move -= LocalUp;
    }

    const float DeltaTime = glm::clamp(io.DeltaTime, 0.f, MaxDeltaTime);
    if (glm::dot(Move, Move) > 0.f && DeltaTime > 0.f)
    {
        // Normalise first so diagonals are not faster, then rotate the input
        // from player space into world space using the camera quaternion.
        Position += Orientation * (glm::normalize(Move) * (GControl.movingSpeed * DeltaTime));
        bCameraChanged = true;
    }

    // ---- Mouse look, composed as quaternions ----
    const bool bDragging = !io.WantCaptureMouse && (io.MouseDown[0] || io.MouseDown[1]);
    const bool bDragJustStarted = bDragging && !GControl.bMouseDragging;
    GControl.bMouseDragging = bDragging;

    if (bDragging && !bDragJustStarted)
    {
        const float DeltaX = glm::clamp(io.MouseDelta.x, -MaxMouseDelta, MaxMouseDelta) * GControl.mouseSensitivity;
        const float DeltaY = glm::clamp(io.MouseDelta.y, -MaxMouseDelta, MaxMouseDelta) * GControl.mouseSensitivity;

        // A local rotation about +Y by -PitchDelta changes the pitch by
        // +PitchDelta, so clamp the delta to what is left before the view flips.
        const float3 Forward = Orientation * LocalForward;
        const float Pitch = std::asin(glm::clamp(glm::dot(Forward, WorldUp), -1.f, 1.f));
        const float PitchDelta = glm::clamp(DeltaY, -MaxPitch - Pitch, MaxPitch - Pitch);

        const glm::quat Yaw = glm::angleAxis(DeltaX, WorldUp);                  // world-space
        const glm::quat PitchRotation = glm::angleAxis(-PitchDelta, LocalLeft); // camera-local
        Orientation = glm::normalize(Yaw * Orientation * PitchRotation);
        bCameraChanged = bCameraChanged || (DeltaX != 0.f || PitchDelta != 0.f);
    }

    // CameraTransformLocalToWorld is what the renderers and shaders consume;
    // rebuild it from the quaternion so rotation can never shear or drift.
    Camera = glm::mat4_cast(Orientation);
    Camera[3] = float4(Position, 1.f);
    GControl.CameraOrientation = Orientation;

    // Path tracing restarts its accumulation whenever the view actually changes.
    GControl.isClick = bCameraChanged;
}

// Scene presets (ModelUniformObject / BuildCornellBoxScene / BuildSponzaScene)
// live in Renderer/Public/PathTraceScene.h and are owned by
// PathTraceRenderer::ApplyScene(). The ImGui dropdown in
// PathTraceRenderer::DrawImGUI() switches between them at runtime; this file
// only decides which one is selected at startup.

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

    // if (RendererSelection == ERendererSelection::PathTracing) {
    //     PTRenderer.CreateRenderer(height, width, BackendSelection, bEnableValidation, Engine::ProcessInput);
    //     PTRenderer.CreateResource();
    //     CameraTransformLocalToWorld[0] = float4(0.f, 1.f, 0.f, 0.f);
    //     CameraTransformLocalToWorld[1] = float4(-1.f, 0.f, 0.f, 0.f);
    //     CameraTransformLocalToWorld[3] = float4(-0.27f, -0.8f, 0.27f, 1.f);
    //     GControl.CameraTransformLocalToWorld = CameraTransformLocalToWorld;
    //
    //     PTRenderer.PTResource.SceneUniform->Initialize(PTRenderer.Env.Context, sizeof(ModelUniformObject), 8, RHIResourceState::BUFFER_UNIFORM);
    //
    //     std::vector<ModelUniformObject> Objects;
    //
    //     // Light
    //     SetSceneUniform(2, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.535f)) * glm::rotate(float4x4(1.0), glm::radians(180.f), float3(0, 1, 0.)) * glm::scale(float4x4(1.0f), float3(0.065f, 0.05f, 1.f) * 2.f), float3(1.f, 1.f, 1.f), float3(1., 1., 1.));
    //
    //     // Floor
    //     SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.27f, 0.001f)), float3(0.8f, 0.8f, 0.8f), float3(0., 0., 0.));
    //     // Celling
    //     SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.54f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.27f, 0.001f)), float3(0.8f, 0.8f, 0.8f), float3(0., 0., 0.));
    //     // Leftwall
    //     SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(-0.54f, 0.27f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.001f, 0.27f, 0.27f)), float3(1.f, 0.f, 0.f), float3(0., 0., 0.));
    //     // Rightwall
    //     SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(0.f, 0.27f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.001f, 0.27f, 0.27f)), float3(0.f, 1.f, 0.f), float3(0., 0., 0.));
    //     // Backwall
    //     SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.54f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.001f, 0.27f)), float3(0.8f, 0.8f, 0.8f), float3(0., 0., 0.));
    //     //// Shortbox
    //     //SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(-0.185f, 0.169f, 0.0825f)) * glm::rotate(float4x4(1.0), glm::radians(-196.62f), float3(0, 0, 1.)) * glm::scale(float4x4(1.0f), float3(0.085f, 0.085f, 0.085f)), float3(0.8f, 0.8f, 0.8f), float3(0., 0., 0.));
    //     //// Tallbox
    //     //SetSceneUniform(1, Objects, glm::translate(float4x4(1.0), float3(-0.368f, 0.351f, 0.165f)) * glm::rotate(float4x4(1.0), glm::radians(-252.77f), float3(0, 0, 1.)) * glm::scale(float4x4(1.0f), float3(0.085f, 0.085f, 0.17f)), float3(0.8f, 0.8f, 0.8f), float3(0., 0., 0.));
    //
    //     // Cow
    //     SetSceneUniform(0, Objects, glm::translate(float4x4(1.0), float3(-0.25f, 0.25f, 0.17f)) *
    //         glm::rotate(float4x4(1.0), glm::radians(180.f+45.f), float3(0, 0, 1)) *
    //         glm::rotate(float4x4(1.0), glm::radians(90.f), float3(1, 0, 0)) *
    //         glm::scale(float4x4(1.0f), float3(0.15f, 0.15f, 0.15f)), float3(1.f, 1.f, 0.f), float3(0., 0., 0.));
    //     PTRenderer.cuo.modelUniformCount = Objects.size();
    //     PTRenderer.PTResource.SceneUniform->CopyToBuffer(PTRenderer.Env.Context, Objects.data(), (uint32_t)(Objects.size() * sizeof(ModelUniformObject)));
    //     PTRenderer.PTResource.PrimitiveBuffer->Initialize(PTRenderer.Env.Context, (uint32_t)(sizeof(ModelUniformObject) * Objects.size()), RHIResourceState::BUFFER_SHADER_STORAGE);
    //     PTRenderer.PTResource.PrimitiveBuffer->CopyToBuffer(PTRenderer.Env.Context, Objects.data(), (uint32_t)(Objects.size() * sizeof(ModelUniformObject)));
    // }
    // else if (RendererSelection == ERendererSelection::BlinnPhong) {
    //     BPRenderer.CreateRenderer(height, width, BackendSelection, bEnableValidation, Engine::ProcessInput);
    //     BPRenderer.CreateResource();
    //     CameraTransformLocalToWorld = glm::translate(float4x4(1.0f), float3(-10.f, 0, 0.0f));
    //     GControl.CameraTransformLocalToWorld = CameraTransformLocalToWorld;
    // }
    // else if (RendererSelection == ERendererSelection::Base) {
    //     BRenderer.CreateRenderer(height, width, BackendSelection, bEnableValidation, Engine::ProcessInput);
    // }
    // else if (RendererSelection == ERendererSelection::Switch)
    {
        // ProcessInput is registered as the per-frame ImGui callback: it is
        // invoked once per frame after ImGui::NewFrame(), when ImGuiIO is fresh.
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
        // The initial transform is a pure rotation (Rz(90 deg)) plus translation,
        // so it can be lifted into the quaternion that drives navigation.
        GControl.CameraOrientation = glm::normalize(glm::quat_cast(CameraTransformLocalToWorld));

        // Scene primitives, mesh BVH and camera preset are now built lazily by
        // PathTraceRenderer::ApplyScene() on the first BeginRender(). This also
        // makes the ImGui scene dropdown a pure data swap at runtime.
        SRenderer.AddRenderer(&PTRenderer, "Path Trace");
        SRenderer.AddRenderer(&BRenderer, "Base");
        SRenderer.AddRenderer(&BPRenderer, "Blinn Phong");
        SRenderer.CurrentSelection = 0;
        if (RendererSelection==ERendererSelection::Base) {
            SRenderer.CurrentSelection = 1;
        }else if (RendererSelection==ERendererSelection::BlinnPhong) {
            SRenderer.CurrentSelection = 2;
        }
    }
}


 void Engine::Run() {
    static auto startTime = std::chrono::high_resolution_clock::now();

    IRHIContext* ActiveContext = nullptr;
    IRenderer* ActiveRenderer = nullptr;
    // if (RendererSelection == ERendererSelection::PathTracing) {
    //     ActiveContext = PTRenderer.Env.Context;
    //     ActiveRenderer = &PTRenderer;
    // } else if (RendererSelection == ERendererSelection::BlinnPhong) {
    //     ActiveContext = BPRenderer.Env.Context;
    //     ActiveRenderer = &BPRenderer;
    // } else if (RendererSelection == ERendererSelection::Base) {
    //     ActiveContext = BRenderer.Env.Context;
    //     ActiveRenderer = &BRenderer;
    // } else if (RendererSelection == ERendererSelection::Switch)
    {
        ActiveContext = SRenderer.Env.Context;
        ActiveRenderer = &SRenderer;
    }

    if (!ActiveContext) return;

    int currentFrame = 0;
    // PathTrace renders one tile per frame. --frame-count therefore counts
    // completed accumulation sweeps (one full image sample per pixel), not
    // render-loop iterations. The hard cap is a runaway guard in case a sweep
    // never completes (e.g. rendering paused so frameId stops advancing).
    auto IsPathTraceActive = [&]() {
        return SRenderer.CurrentSelection >= 0 &&
               SRenderer.CurrentSelection < SRenderer.AddedRenderers.size() &&
               SRenderer.AddedRenderers[SRenderer.CurrentSelection] ==
                   static_cast<BaseRenderer*>(&PTRenderer);
    };
    const long long HardFrameCap = (MaxFrames > 0) ? 1024LL * MaxFrames : -1;

    while (ActiveContext->IsWindowAlive())
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        // Navigation input is handled exactly once per frame by EngineUIFunc,
        // which CreateRenderer() registers as the ImGui UI callback. That runs
        // after ImGui::NewFrame(), so ImGuiIO is fresh there. Calling
        // ProcessInput() here as well applied every key/mouse delta twice and
        // doubled both movement and rotation speed.

        // if (RendererSelection == ERendererSelection::PathTracing) {
        //     PTRenderer.UpdateUniformBuffer(GControl.CameraTransformLocalToWorld, time, &GControl);
        //     PTRenderer.Render(float4(1.f), &GControl);
        //     PTRenderer.ProcessInput();
        // } else if (RendererSelection == ERendererSelection::BlinnPhong) {
        //     BPRenderer.Render(GControl.CameraTransformLocalToWorld[3], &GControl);
        //     BPRenderer.Env.Context->ProcessFrameInput();
        // } else if (RendererSelection == ERendererSelection::Base) {
        //     BRenderer.Render(float4(0.f), &GControl);
        //     BRenderer.Env.Context->ProcessFrameInput();
        // } else if (RendererSelection == ERendererSelection::Switch)
        {
            if (IsPathTraceActive()) {
                PTRenderer.UpdateUniformBuffer(GControl.CameraTransformLocalToWorld, time, &GControl);
                PTRenderer.ProcessInput();
            }
            SRenderer.Render(float4(1.f), &GControl);
            SRenderer.Env.Context->ProcessFrameInput();
        }

        currentFrame++;
        if (MaxFrames > 0)
        {
            const int Completed = IsPathTraceActive()
                ? PTRenderer.GetCompletedSweeps() : currentFrame;
            if (Completed >= MaxFrames || currentFrame >= HardFrameCap)
            {
                if (!OutputPath.empty()) {
                    // SwitchRenderer::CaptureFrame is BaseRenderer's no-op stub;
                    // route to the active sub-renderer so PathTrace can read back
                    // its accumulation image. A completed sweep means every
                    // tile has been written, so the PNG is a full frame.
                    if (IsPathTraceActive())
                        PTRenderer.CaptureFrame(OutputPath);
                    else
                        ActiveRenderer->CaptureFrame(OutputPath);
                }
                break;
            }
        }
    }
}


void Engine::Shutdown() {}

void Engine::SetPathTraceScene(int SceneIndex) {
    PTRenderer.SetPathTraceScene(std::clamp(SceneIndex, 0, (int)EPathTraceScene::Count - 1));
}

void Engine::SetSponzaLOD(int TriangleStride) {
    PTRenderer.SponzaTriangleStride = std::clamp(TriangleStride, 1, 12);
}

void Engine::SetDebugMode(int Mode) {
    PTRenderer.cuo.debugMode = std::clamp(Mode, 0, 2);
}

void Engine::SetTileSize(int TileSize) {
    // Snap arbitrary CLI values down onto the four presets the ImGui combo
    // offers; a different grid length invalidates the current sweep.
    int Index = 0;
    for (int i = 0; i < 4; ++i)
        if (TileSize >= PathTraceRenderer::kTileSizes[i]) Index = i;
    PTRenderer.TileSizeIndex = std::clamp(Index, 0, 3);
    PTRenderer.ResetAccumulation();
}

 int main(int argc, char** argv) {
    ERendererSelection renderer = ERendererSelection::Switch;
    RHIBackend rhi = RHIBackend::Vulkan;
    int maxFrames = -1;
    std::string outputPath = "";
    bool enableValidation = true;
    int pathTraceScene = (int)EPathTraceScene::CornellBox;
    int sponzaStride = 4;
    int debugMode = 0;
    int tileSize = -1;
    bool bLodSpecified = false;

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
        } else if (arg == "--scene" && i + 1 < argc) {
            std::string val = argv[++i];
            if (val == "CornellBox" || val == "cornellbox") pathTraceScene = (int)EPathTraceScene::CornellBox;
            else if (val == "Sponza" || val == "sponza") pathTraceScene = (int)EPathTraceScene::Sponza;
        } else if (arg == "--normal-debug") {
            debugMode = 1;   // primary ray -> flat triangle normal
        } else if (arg == "--normal-depth-debug") {
            debugMode = 2;   // same, plus a distance-based depth cue
        } else if (arg == "--sponza-lod" && i + 1 < argc) {
            // Tile accumulation splits the frame into one small dispatch per
            // submit, so stride 1 (full 262k triangles) is safe with shading.
            sponzaStride = std::clamp(std::stoi(argv[++i]), 1, 12);
            bLodSpecified = true;
        } else if (arg == "--tile-size" && i + 1 < argc) {
            // Smaller tiles = fewer threads per dispatch = more TDR headroom.
            tileSize = std::stoi(argv[++i]);
        } else if (arg == "-nsprofile") {
            enableValidation = false;
        }
    }

    // Normal-debug issues a single ray per pixel, so full Sponza detail is
    // safe there and gives a much cleaner geometry view. Argument order is
    // irrelevant because this is resolved after parsing.
    if (debugMode != 0 && !bLodSpecified)
        sponzaStride = 1;
    if (debugMode == 0 && sponzaStride < 4)
        std::cout << "[Engine] --sponza-lod " << sponzaStride
                  << " (full detail) is safe with tile accumulation: one tile "
                     "per submit, --frame-count counts full sweeps." << std::endl;

    Engine engine;
    engine.Initialize(1000, 1000, renderer, rhi, maxFrames, outputPath, enableValidation);
    engine.SetPathTraceScene(pathTraceScene);
    engine.SetSponzaLOD(sponzaStride);
    engine.SetDebugMode(debugMode);
    if (tileSize > 0)
        engine.SetTileSize(tileSize);
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