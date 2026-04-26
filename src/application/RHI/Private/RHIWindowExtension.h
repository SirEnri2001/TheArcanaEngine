#pragma once
#define RHI_INCLUDE
#include "RHI.h"
#include "vulkan/vulkan_core.h"
class IRHIWindowExtension {
public:
    virtual ~IRHIWindowExtension() {}
    virtual void HookBeginContextInit() = 0;

    virtual void HookBeforeSurfaceInit() = 0;

    virtual void HookAfterSurfaceInit() = 0;
    virtual void ProcessMessages() = 0;

    virtual bool IsWindowAlive() = 0;
    virtual void* GetHWND() = 0;
    virtual void HookImGuiInit(RHIBackend Backend) = 0;
    virtual void HookImGuiNewFrame() = 0;
    virtual void Cleanup() = 0;
    virtual void InitializeWindow(uint32_t width, uint32_t height, const char* title) = 0;
    virtual void CreateVkSurface(void* Instance, void** OutVkSurface) = 0;
};

class RHIGLFWExtension : public IRHIWindowExtension {
public:
    RHIGLFWExtension();
    virtual void HookBeginContextInit() override;
    virtual void HookBeforeSurfaceInit() override;
    virtual void HookAfterSurfaceInit() override;
    virtual void ProcessMessages() override;
    virtual bool IsWindowAlive() override;
    virtual void* GetHWND() override;
    virtual void HookImGuiInit(RHIBackend Backend) override;
    virtual void HookImGuiNewFrame() override;
    virtual void Cleanup() override;
    virtual void InitializeWindow(uint32_t width, uint32_t height, const char* title) override;
    virtual void CreateVkSurface(void* Instance, void** OutVkSurface) override;
private:
    void* pWindow; // GLFWwindow*
};
class RHIHWNDExtension : public IRHIWindowExtension {
public:
    RHIHWNDExtension();
    virtual void HookBeginContextInit() override;
    virtual void HookBeforeSurfaceInit() override;
    virtual void HookAfterSurfaceInit() override;
    virtual void ProcessMessages() override;
    virtual bool IsWindowAlive() override;
    virtual void* GetHWND() override;
    virtual void HookImGuiInit(RHIBackend Backend) override;
    virtual void HookImGuiNewFrame() override;
    virtual void Cleanup() override;
    virtual void InitializeWindow(uint32_t width, uint32_t height, const char* title) override;
    virtual void CreateVkSurface(void* Instance, void** OutVkSurface) override;
private:
    void* hWnd;    // HWND
    bool bAlive = true;
};


std::unique_ptr<IRHIWindowExtension> CreateWindowExtension(WindowExtensionSelection Selection);