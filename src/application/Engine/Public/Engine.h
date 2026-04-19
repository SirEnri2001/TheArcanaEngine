#pragma once

#include "RHI.h"
#include "RHIImGuiHelper.h"
#include "Renderer.h"
#include "PathTraceRenderer.h"
#define BLINNPHONGRENDERER_INCLUDE
#include "BlinnPhongRenderer.h"

enum class ERendererSelection {
    PathTracing,
    BlinnPhong
};

class Engine {
public:
    Engine();
    ~Engine();

     void Initialize(int width, int height, ERendererSelection Renderer, RHIBackend Backend, int MaxFrames = -1, const std::string& OutputPath = "", bool bEnableValidation = true);
    void Run();
    void Shutdown();

private:    
    static void DrawUI(ImGuiSharedGlobals* ImGlobals);

    static RenderControl GControl;
    ERendererSelection RendererSelection;
    RHIBackend BackendSelection;
    int MaxFrames;
    std::string OutputPath;

    PathTraceRenderer PTRenderer;
    BlinnPhongRenderer BPRenderer;
    float4x4 CameraTransformLocalToWorld;
};

