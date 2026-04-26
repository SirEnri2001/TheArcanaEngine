#include "BaseRenderer.h"
#include <iostream>

#include "imgui.h"
#include "RHIImGuiHelper.h"

void BaseRenderer::CreateRenderer(uint32_t Height, uint32_t Width, RHIBackend Backend, bool bEnableValidation, std::function<void(ImGuiSharedGlobals*)> InEngineUIFunc) {
    EngineUIFunc = InEngineUIFunc;
    Resource.uptr_Context = IRHIPlatformSupport::Get(Backend)->CreateRHIContext();
    Env.Context = Resource.uptr_Context.get();

    ContextCreateParams Params;
    Params.WindowWidth = Width;
    Params.WindowHeight = Height;
    Params.bEnableValidation = bEnableValidation;
    Env.Context->Initialize(Params);
    Resource.Swapchain = Env.Context->CreateRHISwapchain();
    Resource.RenderPass = Env.Context->CreateRHIRenderPass();
    Resource.ImGUI = Env.Context->CreateRHIImGUI();
    Env.SwapchainFormat = B8G8R8A8_SRGB;
    Resource.Swapchain->Initialize(Env.Context, Env.SwapchainFormat);
    std::vector<RHIFormat> ColorRTFormats = { Env.SwapchainFormat };
    Resource.RenderPass->Initialize(Env.Context, ColorRTFormats);
    Env.FrameSize = Resource.Swapchain->GetFrameSize();
    Resource.ImGUI->Initialize(Env.Context, Resource.Swapchain.get(), Resource.RenderPass.get());

    Env.Swapchain = Resource.Swapchain.get();
    Env.RenderPass = Resource.RenderPass.get();
    Env.ImGUI = Resource.ImGUI.get();
}

void BaseRenderer::GetEnv(RendererEnvironment& OutEnv)
{
    OutEnv = Env;
}

void BaseRenderer::SetEnv(const RendererEnvironment& InEnv)
{
    Env = InEnv;
}


void BaseRenderer::Render(float4 ViewPos, RenderControl* control) {
    RenderUI();

    auto CommandBuffer = Env.Context->CreateRHICommandBuffer();
    CommandBuffer->Initialize(Env.Context);

    IRHIFrameBuffer* CurrentFrameBuffer = nullptr;
    if (!BeginRender(CommandBuffer.get(), CurrentFrameBuffer, control)) {
        return;
    }

    ResizeScreen();

    DrawPasses(CommandBuffer.get(), CurrentFrameBuffer, ViewPos, control);

    EndRender(CommandBuffer.get());
}

void BaseRenderer::RenderUI() {
    if (Env.ImGUI) {
        Env.ImGUI->BeginImGUI([this](ImGuiSharedGlobals* ImGlobals)
            {
                ImGui::SetCurrentContext(ImGlobals->Context);
                ImGui::SetAllocatorFunctions(ImGlobals->MemAllocFunc, ImGlobals->MemFreeFunc, ImGlobals->UserData);
            });
        Env.ImGUI->DrawImGUI([this]()
        {
                if (EngineUIFunc) EngineUIFunc(Env.ImGUI->GetGlobals());
                this->DrawImGUI();
        });
        Env.ImGUI->EndImGUI();
    }
}

bool BaseRenderer::BeginRender(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer*& OutFrameBuffer, RenderControl* control) {
    CommandBuffer->BeginCommandBuffer();
    Env.Swapchain->AcquireFrame(Env.Context, OutFrameBuffer, Env.RenderPass);
    if (!OutFrameBuffer) {
        return false;
    }
    return true;
}

void BaseRenderer::ResizeScreen() {
    if (Env.FrameSize != Env.Swapchain->GetFrameSize()) {
        Env.FrameSize = Env.Swapchain->GetFrameSize();
    }
}

void BaseRenderer::DrawPasses(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer* FrameBuffer, float4 ViewPos, RenderControl* control) {
    Env.RenderPass->BeginRenderPass(CommandBuffer, FrameBuffer);
    if (Env.ImGUI) {
        Env.ImGUI->DispatchImGUI(CommandBuffer);
    }
    Env.RenderPass->EndRenderPass(CommandBuffer);
}

void BaseRenderer::EndRender(IRHICommandBuffer* CommandBuffer) {
    CommandBuffer->EndCommandBuffer();
    Env.Swapchain->PresentFrameAndRelease(Env.Context, CommandBuffer);
    Env.Context->WaitDeviceIdle();
}

void BaseRenderer::CaptureFrame(const std::string& Path) {
    std::cout << "CaptureFrame not implemented for BaseRenderer" << std::endl;
}

void BaseRenderer::DrawImGUI()
{
    static bool check = false;
    ImGui::Begin("Base Renderer");
    ImGui::Checkbox("Check", &check);
    ImGui::End();
}


void SwitchRenderer::AddRenderer(BaseRenderer* Renderer, const char* RendererName)
{
    AddedRenderers.push_back(Renderer);
    RendererNames.push_back(RendererName);
}

void SwitchRenderer::DrawImGUI()
{
    ImGui::Begin("Switch Renderer");
    ImGui::Text("Select Renderer: ");
    if (ImGui::BeginCombo("##RendererSelection", CurrentSelection >= 0 && CurrentSelection < RendererNames.size() ? RendererNames[CurrentSelection] : "None"))
    {
        for (int i = 0; i < AddedRenderers.size(); i++)
        {
            const bool is_selected = (CurrentSelection == i);
            if (ImGui::Selectable(RendererNames[i], is_selected))
            {
                CurrentSelection = i;
            }
            if (is_selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::End();

    if (CurrentSelection >= 0 && CurrentSelection < AddedRenderers.size()) {
        AddedRenderers[CurrentSelection]->DrawImGUI();
    }
}

bool SwitchRenderer::BeginRender(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer*& OutFrameBuffer, RenderControl* control) {
    if (CurrentSelection >= 0 && CurrentSelection < AddedRenderers.size()) {
        return AddedRenderers[CurrentSelection]->BeginRender(CommandBuffer, OutFrameBuffer, control);
    }
    return BaseRenderer::BeginRender(CommandBuffer, OutFrameBuffer, control);
}

void SwitchRenderer::ResizeScreen() {
    if (CurrentSelection >= 0 && CurrentSelection < AddedRenderers.size()) {
        AddedRenderers[CurrentSelection]->ResizeScreen();
    } else {
        BaseRenderer::ResizeScreen();
    }
}

void SwitchRenderer::DrawPasses(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer* FrameBuffer, float4 ViewPos, RenderControl* control) {
    if (CurrentSelection >= 0 && CurrentSelection < AddedRenderers.size()) {
        AddedRenderers[CurrentSelection]->DrawPasses(CommandBuffer, FrameBuffer, ViewPos, control);
    } else {
        BaseRenderer::DrawPasses(CommandBuffer, FrameBuffer, ViewPos, control);
    }
}

void SwitchRenderer::EndRender(IRHICommandBuffer* CommandBuffer) {
    if (CurrentSelection >= 0 && CurrentSelection < AddedRenderers.size()) {
        AddedRenderers[CurrentSelection]->EndRender(CommandBuffer);
    } else {
        BaseRenderer::EndRender(CommandBuffer);
    }
}

