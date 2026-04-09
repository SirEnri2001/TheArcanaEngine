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

    void Initialize(int width, int height);
    void Run();
    void Shutdown();

private:    
    static void DrawUI(ImGuiSharedGlobals* ImGlobals);

    static RenderControl GControl;
    static ERendererSelection RendererSelection;

    PathTraceRenderer PTRenderer;
    BlinnPhongRenderer BPRenderer;
    float4x4 CameraTransformLocalToWorld;
};

