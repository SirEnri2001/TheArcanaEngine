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

