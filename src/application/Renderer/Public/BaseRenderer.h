#pragma once

#include <string>
#include <vector>
#include <memory>

#define RENDERER_IMPLEMENT
#include "Renderer.h"

class RENDERER_API BaseRenderer : public IRenderer {
public:
    friend class SwitchRenderer;

    struct OwnedResource
    {
        std::unique_ptr<IRHIContext> uptr_Context;
        std::unique_ptr<IRHISwapchain> Swapchain;
        std::unique_ptr<IRHIRenderPass> RenderPass;
        std::unique_ptr<IRHIImGUI> ImGUI;
    } Resource;

    RendererEnvironment Env;

    std::function<void(ImGuiSharedGlobals*)> EngineUIFunc;

    BaseRenderer() = default;

     // --- IRenderer interface ---
    virtual void CreateRenderer(uint32_t Height, uint32_t Width, RHIBackend Backend, bool bEnableValidation = true, std::function<void(ImGuiSharedGlobals*)> InEngineUIFunc = nullptr) override;
    virtual void Render(float4 ViewPos, RenderControl* control) override;
    virtual void CaptureFrame(const std::string& Path) override;
    virtual void DrawImGUI() override;

    virtual void GetEnv(RendererEnvironment& OutEnv) override;
    virtual void SetEnv(const RendererEnvironment& InEnv) override;

public:
    virtual void RenderUI();
    virtual bool BeginRender(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer*& OutFrameBuffer, RenderControl* control);
    virtual void ResizeScreen();
    virtual void DrawPasses(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer* FrameBuffer, float4 ViewPos, RenderControl* control);
    virtual void EndRender(IRHICommandBuffer* CommandBuffer);

};

class RENDERER_API SwitchRenderer : public BaseRenderer
{
public:
    std::vector<BaseRenderer*> AddedRenderers;
    std::vector<const char*> RendererNames;
    int CurrentSelection = -1;
    // BaseRenderer
    void AddRenderer(BaseRenderer* Renderer, const char* RendererName);
    virtual void DrawImGUI() override;

protected:
    virtual bool BeginRender(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer*& OutFrameBuffer, RenderControl* control) override;
    virtual void ResizeScreen() override;
    virtual void DrawPasses(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer* FrameBuffer, float4 ViewPos, RenderControl* control) override;
    virtual void EndRender(IRHICommandBuffer* CommandBuffer) override;
};