#pragma once

#include "RHI.h"
#include "RHIImGuiHelper.h"
#include "Renderer.h"
#include "PathTraceRenderer.h"

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
    PathTraceRenderer PTRenderer;
    float4x4 CameraTransformLocalToWorld;
};
