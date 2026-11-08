#pragma once

#include "RHI.h"
#include "RHIImGuiHelper.h"
#define RENDERER_INCLUDE
#include "Renderer.h"
#include "PathTraceRenderer.h"
#include "BlinnPhongRenderer.h"
#include "BaseRenderer.h"

enum class ERendererSelection {
    PathTracing,
    BlinnPhong,
    Base,
    Switch
};

class Engine {
public:
    Engine();
    ~Engine();

	void Initialize(int width, int height, ERendererSelection Renderer, RHIBackend Backend, int MaxFrames = -1, const std::string& OutputPath = "", bool bEnableValidation = true);
    void Run();
    void Shutdown();

    // Initial PathTrace scene for the ImGui dropdown (also exposed as --scene).
    void SetPathTraceScene(int SceneIndex);
    void SetSponzaLOD(int TriangleStride);
    // 0 = path traced, 1 = flat triangle normals, 2 = normals + depth cue
    void SetDebugMode(int Mode);
    // Edge length of one compute tile in pixels (snapped to 64/128/256/512).
    void SetTileSize(int TileSize);

private:    
    static void ProcessInput(ImGuiSharedGlobals* ImGlobals);

    static RenderControl GControl;
    ERendererSelection RendererSelection;
    RHIBackend BackendSelection;
    int MaxFrames;
    std::string OutputPath;

    PathTraceRenderer PTRenderer;
    BlinnPhongRenderer BPRenderer;
    BaseRenderer BRenderer;
    SwitchRenderer SRenderer;
    float4x4 CameraTransformLocalToWorld;
};

