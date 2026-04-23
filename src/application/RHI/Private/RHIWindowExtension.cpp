#include "RHIWindowExtension.h"
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#define RHI_INCLUDE
#include "RHI.h"
#include "imgui.h"
#include <imgui_impl_glfw.h>
#include <stdexcept>
#ifdef _WIN32
#include "imgui_impl_win32.h"
#endif
#include <vulkan/vulkan_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- RHIGLFWExtension ---
RHIGLFWExtension::RHIGLFWExtension()
{
    pWindow = nullptr;
}

void RHIGLFWExtension::HookBeginContextInit() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // add hint immediately after Init otherwise create surface will fail
}
void RHIGLFWExtension::HookBeforeSurfaceInit() {
}

void RHIGLFWExtension::InitializeWindow(uint32_t width, uint32_t height, const char* title)
{
    GLFWwindow* pGLFWwindow = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(pGLFWwindow, this);
    //glfwSetFramebufferSizeCallback(pGLFWwindow, framebufferResizeCallback);
    pWindow = pGLFWwindow;
}

void RHIGLFWExtension::CreateVkSurface(VkInstance& Instance, VkSurfaceKHR& OutVkSurface)
{
    if (glfwCreateWindowSurface(Instance, (GLFWwindow*)pWindow, nullptr, &OutVkSurface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void RHIGLFWExtension::HookAfterSurfaceInit() {
    if (pWindow) {
        glfwShowWindow((GLFWwindow*)pWindow);
    }
}
void RHIGLFWExtension::ProcessMessages() {
    glfwPollEvents();
}
bool RHIGLFWExtension::IsWindowAlive() {
    return pWindow && !glfwWindowShouldClose((GLFWwindow*)pWindow);
}
void* RHIGLFWExtension::GetHWND() {
    return glfwGetWin32Window((GLFWwindow*)pWindow);
}
void RHIGLFWExtension::HookImGuiInit(RHIBackend Backend) {
    if (Backend == RHIBackend::Vulkan) {
        ImGui_ImplGlfw_InitForVulkan((GLFWwindow*)pWindow, true);
    }
    else if (Backend == RHIBackend::D3D12) {
        ImGui_ImplGlfw_InitForOther((GLFWwindow*)pWindow, true);
    }
}
void RHIGLFWExtension::HookImGuiNewFrame() {
    ImGui_ImplGlfw_NewFrame();
}
void RHIGLFWExtension::Cleanup() {
    if (pWindow) {
        glfwDestroyWindow((GLFWwindow*)pWindow);
        pWindow = nullptr;
    }
}

RHIHWNDExtension::RHIHWNDExtension() {}
void RHIHWNDExtension::HookBeginContextInit() {
#ifdef _WIN32
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;
    RECT* rect;
    switch (message)
    {
    case WM_SIZING:
        rect = reinterpret_cast<RECT*>(lParam); // Not being used yet
        return 0;
    case WM_SIZE:
        //Context->SetResized(true, lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

void RHIHWNDExtension::InitializeWindow(uint32_t width, uint32_t height, const char* title)
{
    // Use HWND for window creation
    HINSTANCE hInstance = GetModuleHandle(NULL);

    // Initialize the window class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = "DXSampleClass";
	RegisterClassEx(&windowClass);
    //RECT windowRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    //AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);


    // Create the main window. 
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    RECT rect = { 0, 0, (LONG)width, (LONG)height };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    hWnd = CreateWindow(
        "DXSampleClass",        // name of window class 
        "TheArcanaEngine - D3D12 API",            // title-bar string 
        WS_OVERLAPPEDWINDOW, // top-level window 
        CW_USEDEFAULT,       // default horizontal position 
        CW_USEDEFAULT,       // default vertical position 
        rect.right - rect.left,
        rect.bottom - rect.top,
        (HWND)NULL,         // no owner window 
        (HMENU)NULL,        // use class menu 
        hInstance,           // handle to application instance 
        (LPVOID)NULL);      // no window-creation data 
    SetWindowLongPtr((HWND)hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    //ShowWindow((HWND)hWnd, 1);
}

void RHIHWNDExtension::HookBeforeSurfaceInit() {

}
void RHIHWNDExtension::HookAfterSurfaceInit() {
#ifdef _WIN32
    if (hWnd) {
        ShowWindow((HWND)hWnd, SW_SHOW);
        UpdateWindow((HWND)hWnd);
    }
#endif
}
void RHIHWNDExtension::ProcessMessages() {
#ifdef _WIN32
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            bAlive = false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif
}
bool RHIHWNDExtension::IsWindowAlive() {
    return bAlive;
}
void* RHIHWNDExtension::GetHWND() {
    return hWnd;
}

void RHIHWNDExtension::CreateVkSurface(VkInstance& Instance, VkSurfaceKHR& OutVkSurface)
{
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = (HWND)hWnd; // Handle to the window
    createInfo.hinstance = GetModuleHandle(nullptr);

    if (vkCreateWin32SurfaceKHR(Instance, &createInfo, nullptr, &OutVkSurface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Win32 surface!");
    }
}


void RHIHWNDExtension::HookImGuiInit(RHIBackend Backend) {
#ifdef _WIN32
    ImGui_ImplWin32_Init((HWND)hWnd);
#endif
}
void RHIHWNDExtension::HookImGuiNewFrame() {
#ifdef _WIN32
    ImGui_ImplWin32_NewFrame();
#endif
}
void RHIHWNDExtension::Cleanup() {
#ifdef _WIN32
    if (hWnd) {
        ::DestroyWindow((HWND)hWnd);
        hWnd = nullptr;
    }
#endif
}

std::unique_ptr<IRHIWindowExtension> CreateWindowExtension(WindowExtensionSelection Selection)
{
    if (Selection == WindowExtensionSelection::GLFW)
    {
        return std::make_unique<RHIGLFWExtension>();
    }
    if (Selection == WindowExtensionSelection::HWND)
    {
        return std::make_unique<RHIHWNDExtension>();
    }
    return std::make_unique<RHIGLFWExtension>();
}
