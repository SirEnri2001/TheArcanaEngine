#define COREGEOMETRY_INCLUDE
#define CORESCENE_INCLUDE
#define RENDERER_INCLUDE
#define PBR_RENDERER_INCLUDE

#include <chrono>
#include <fstream>

#include "CoreMath.h"
#include "CoreMath.inl"
#include "CoreGeometry.h"
#include "CoreScene.h"
#include "CoreLog.inl"
#include "PBRRenderer.h"
#include "Renderer.h"
#include "RHIImGuiHelper.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const std::string MODEL_PATH = "models/spot/spot_triangulated_good.obj";
const std::string TEXTURE_PATH = "models/spot/spot_texture.png";
const std::string MODEL2_PATH = "models/viking_room.obj";
const std::string TEXTURE2_PATH = "textures/viking_room.png";
const std::string VERT_SHADER_PATH = "shaderbytecode/glsl/BlinnPhong.vert";
const std::string FRAG_SHADER_PATH = "shaderbytecode/glsl/BlinnPhong.frag";
const std::string CUBE_PATH = "models/cube/cube.obj";

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}
float4 ViewPos = { 2.,2.,2.,1. };
float4 ViewForward;
float4 ViewUp;
void DrawUI(ImGuiSharedGlobals* ImGlobals)
{
    ImGui::SetCurrentContext(ImGlobals->Context);
    ImGui::SetAllocatorFunctions(ImGlobals->MemAllocFunc, ImGlobals->MemFreeFunc, ImGlobals->UserData);
    static bool show_demo_window = false;
    static bool show_another_window = false;
    static float4 clear_color;
    static ImGuiIO& io = ImGui::GetIO();
    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
    {
        ImGui::ShowDemoWindow(&show_demo_window);
    }

    // 2. Show a simple window that we create ourselves. W e use a Begin/End pair to create a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::End();
    }

    // 3. Show another simple window.
    if (show_another_window)
    {
        ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        ImGui::Text("io.WantCaptureMouse = %d", io.WantCaptureMouse);
        if (ImGui::Button("Close Me"))
            show_another_window = false;
        ImGui::End();
    }
    if (!io.WantCaptureMouse && (io.MouseDown[0] || io.MouseDown[1])) // User operate with actual scene
    {
        float DeltaX = io.MousePos.x - io.MousePosPrev.x;
        float DeltaY = io.MousePos.y - io.MousePosPrev.y;

        DeltaX *= 0.25;
        DeltaY *= 0.25;
        auto viewMat = glm::mat4(1.f);
        viewMat = glm::rotate(glm::mat4(1.f), DeltaX * 0.01f, float3(ViewUp)) * viewMat;
        viewMat = glm::rotate(glm::mat4(1.f), DeltaY * 0.01f, glm::cross(float3(ViewForward), float3(ViewUp))) * viewMat;

        float3 PlayerMove = { 0.f, 0.f, 0.f };
        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            PlayerMove += float3(ViewForward);
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            PlayerMove -= float3(ViewForward);
        }
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            PlayerMove -= glm::cross(float3(ViewForward), float3(ViewUp));
        }
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            PlayerMove += glm::cross(float3(ViewForward), float3(ViewUp));
        }
        if (ImGui::IsKeyDown(ImGuiKey_E))
        {
            PlayerMove -= float3(ViewUp);
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q))
        {
            PlayerMove += float3(ViewUp);
        }
        viewMat = glm::translate(glm::mat4(1.0), PlayerMove * 0.001f) * viewMat;
        ViewPos = viewMat * ViewPos;
        ViewForward = viewMat * ViewForward;
        ViewUp = float4(0., 0., 1., 0.);
        ViewForward = float4(glm::normalize(glm::cross(float3(ViewUp), glm::cross(float3(ViewForward), float3(ViewUp)))), 0.0);
    }
}

int PBRRendererTest();

class RenderViewport {
public:
    RHIContext Context;
    RHIWindowManager WindowManager;

    RenderViewport() {

    }

    void InitializeViewport(int Width, int Height) {
        RHIPlatformSupport::Get()->Initialize();
        WindowManager.Initialize(RHIPlatformSupport::Get(), Height, Width);
        Context.Initialize(RHIPlatformSupport::Get());
        WindowManager.InitializeSwapchain(&Context, RHIPlatformSupport::Get());
    }

    bool IsWindowAlive() {
        return WindowManager.IsAlive();
    }
};

class PassPipeline {
public:
    virtual void InitializePipeline(RenderViewport& Viewport, RHIRenderPass& RenderPass) = 0;
};

class BlinnPhongPipeline : public PassPipeline {
public:
    RHIPipelineFactory PipelineFactory;
    RHIPipelineObject PipelineObject;
    RHIPipelineObject PresentPipelineObject;

    std::vector<char> VertexShaderSPIRV;
    std::vector<char> FragmentShaderSPIRV;

    BlinnPhongPipeline() {
        VertexShaderSPIRV = readFile(VERT_SHADER_PATH);
        FragmentShaderSPIRV = readFile(FRAG_SHADER_PATH);
    }

    virtual void InitializePipeline(RenderViewport& Viewport, RHIRenderPass& RenderPass) override {
        PipelineFactory.SetShaders(VertexShaderSPIRV, FragmentShaderSPIRV);
        PipelineFactory.SetUniformBinding(0);
        PipelineFactory.SetImageSamplerBinding(1);
        PipelineFactory.SetUniformBinding(2);
        PipelineFactory.AddBufferBinding(0, sizeof(Mesh::VertexType));
        PipelineFactory.AddBufferLayout(0, 0, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Position));
        PipelineFactory.AddBufferLayout(0, 1, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Color));
        PipelineFactory.AddBufferLayout(0, 2, R32G32_SFLOAT, offsetof(Mesh::VertexType, TexCoord));
        PipelineFactory.AddBufferLayout(0, 3, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Normal));
        PipelineFactory.InitializePipelineObject(&PipelineObject, &Viewport.Context, &RenderPass);
    }
};

class BlinnPhongRenderer {
public:
    uint32_t IndexBufferSize;
    RHIBufferResource RHIFullScreenQuadBuffer;
    RHIBufferResource RHIFullScreenQuadIndexBuffer;
    RHIGraphicsDispatcher GraphicDispatcher;
    RHIRenderPass PresentPass;
    RHIUniform Uniform;

    BlinnPhongPipeline Pipeline;

    RHIImGUI ImGUI;
    void (*pFuncImDraw)(ImGuiSharedGlobals*);

    struct MeshDesc {
        MeshRenderProxy* Proxy;
        RHIUniform* ModelUniform;
    };

    std::vector<MeshDesc> MeshDescs;

    struct UniformBufferObject {
        alignas(16) float4x4 view;
        alignas(16) float4x4 proj;
        alignas(16) float4 viewPosition;
    } ubo;

    BlinnPhongRenderer() {
        
    }

    virtual void CreateRenderer(RenderViewport& Viewport) {
        // create renderer

        Viewport.WindowManager.InitializeRenderPassAsPresent(&PresentPass, &Viewport.Context);

        GraphicDispatcher.Initialize(&Viewport.Context);

        Pipeline.InitializePipeline(Viewport, PresentPass);

        RHIFullScreenQuadBuffer.Initialize(&Viewport.Context, sizeof(float3), 4, BufferType::VERTEX);
        RHIFullScreenQuadIndexBuffer.Initialize(&Viewport.Context, sizeof(uint32_t), 6, BufferType::INDEX);
        float3 FullScreenVertices[4] = { float3(-1, -1, 1), float3(-1, 1, 1), float3(1, 1, 1), float3(1, -1, 1) };
        uint32_t FullScreenVerticesIndex[6] = { 0, 1, 2, 0, 2, 3 };
        RHIFullScreenQuadBuffer.CopyToBuffer(&Viewport.Context, FullScreenVertices, sizeof(float3) * 4);
        RHIFullScreenQuadIndexBuffer.CopyToBuffer(&Viewport.Context, FullScreenVerticesIndex, sizeof(uint32_t) * 6);
        Uniform.Initialize(&Viewport.Context, sizeof(UniformBufferObject));
        ImGUI.Initialize(&Viewport.Context, &Viewport.WindowManager, &PresentPass);

    }

    void UpdateUniformBuffer(RenderViewport& Viewport, glm::mat4 ViewMat, float4 ViewPos) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
    }

    virtual void Render(RenderViewport& Viewport, glm::mat4 ViewMat, float4 ViewPos) {
        auto& Context = Viewport.Context;
        ImGUI.UpdateUI(pFuncImDraw);
        GraphicDispatcher.WaitForGPUIdle(&Context);
        GraphicDispatcher.BeginFrame(&Context, &Viewport.WindowManager, &PresentPass);
        GraphicDispatcher.BeginRenderPass(&PresentPass);
        for (auto& MeshDesc : MeshDescs)
        {
            UpdateUniformBuffer(Viewport, ViewMat, ViewPos);
            Uniform.CopyToBuffer(&Viewport.Context, &ubo, sizeof(ubo));
            Pipeline.PipelineObject.SetUniform(&Uniform, 0);
            Pipeline.PipelineObject.SetImageSampler(&MeshDesc.Proxy->Texture, 1);
            Pipeline.PipelineObject.SetUniform(MeshDesc.ModelUniform, 2);
            GraphicDispatcher.BindIndexBuffer(&MeshDesc.Proxy->RHIIndexBuffer, 0);
            GraphicDispatcher.BindVertexBuffer(&MeshDesc.Proxy->RHIVertexBuffer, 0, 0);
            IndexBufferSize = MeshDesc.Proxy->IndexBufferSize;
            GraphicDispatcher.Dispatch(&Pipeline.PipelineObject, IndexBufferSize, 0, 1);
        }
        // Comment this line if you don't want ImGUI
        ImGUI.DispatchImGUI(&GraphicDispatcher);
        GraphicDispatcher.EndRenderPass(&PresentPass);
        GraphicDispatcher.EndFrameAndSubmit(&Context, &Viewport.WindowManager);
    }

    virtual void DrawMesh(MeshRenderProxy& Proxy, RHIUniform& ModelUniform) {
        MeshDescs.push_back(MeshDesc{&Proxy, &ModelUniform });
    }
};

class PathTracerPipeline : public PassPipeline {
public:
    RHIPipelineFactory PipelineFactory;
    RHIPipelineObject PipelineObject;
    RHIPipelineObject PresentPipelineObject;

    std::vector<char> VertexShaderSPIRV;
    std::vector<char> FragmentShaderSPIRV;

    PathTracerPipeline() {
        VertexShaderSPIRV = readFile("shaderbytecode/glsl/PathTracer.vert");
        FragmentShaderSPIRV = readFile("shaderbytecode/glsl/PathTracer.frag");
    }

    virtual void InitializePipeline(RenderViewport& Viewport, RHIRenderPass& RenderPass) override {
        PipelineFactory.SetShaders(VertexShaderSPIRV, FragmentShaderSPIRV);
        PipelineFactory.SetUniformBinding(0);
        PipelineFactory.SetUniformBinding(1);
        PipelineFactory.AddBufferBinding(0, sizeof(float3));
        PipelineFactory.AddBufferLayout(0, 0, R32G32B32_SFLOAT, 0);
        PipelineFactory.InitializePipelineObject(&PipelineObject, &Viewport.Context, &RenderPass);
    }
};

class PathTraceRenderer {
public:
    uint32_t IndexBufferSize;
    RHIBufferResource RHIFullScreenQuadBuffer;
    RHIBufferResource RHIFullScreenQuadIndexBuffer;
    RHIGraphicsDispatcher GraphicDispatcher;
    RHIRenderPass PresentPass;
    RHIUniform CameraUniform;
    RHIUniform* SceneUniform = nullptr;

    PathTracerPipeline Pipeline;

    RHIImGUI ImGUI;
    void (*pFuncImDraw)(ImGuiSharedGlobals*);

    struct CameraUniformObject {
        alignas(16) float3 eye;
        alignas(16) float3 forward;
        alignas(16) float3 up;
        alignas(8) glm::ivec2 screenres; // not sure 8 or 16 to use here, renderdoc says shader uses 8
        alignas(8) float time;
    } cuo;

    PathTraceRenderer() {

    }

    virtual void CreateRenderer(RenderViewport& Viewport) {
        // create renderer

        Viewport.WindowManager.InitializeRenderPassAsPresent(&PresentPass, &Viewport.Context);

        GraphicDispatcher.Initialize(&Viewport.Context);

        Pipeline.InitializePipeline(Viewport, PresentPass);

        RHIFullScreenQuadBuffer.Initialize(&Viewport.Context, sizeof(float3), 8, BufferType::VERTEX);
        RHIFullScreenQuadIndexBuffer.Initialize(&Viewport.Context, sizeof(uint32_t), 6, BufferType::INDEX);
        float3 FullScreenVertices[4] = { float3(-1., -1., 0.), float3(-1., 1., 0.), float3(1., 1., 0.), float3(1., -1., 0.) };
        uint32_t FullScreenVerticesIndex[6] = { 0, 1, 2, 0, 2, 3 };
        RHIFullScreenQuadBuffer.CopyToBuffer(&Viewport.Context, FullScreenVertices, sizeof(float3) * 8);
        RHIFullScreenQuadIndexBuffer.CopyToBuffer(&Viewport.Context, FullScreenVerticesIndex, sizeof(uint32_t) * 6);
        CameraUniform.Initialize(&Viewport.Context, sizeof(CameraUniformObject));
        ImGUI.Initialize(&Viewport.Context, &Viewport.WindowManager, &PresentPass);

    }

    void UpdateUniformBuffer(float3 ViewPos, float3 ViewForward, float3 ViewUp, float time) {
        // sensor size 32mm
        // focal length 45mm
        // fov = 2 * atan(sensor_size/2/focal_length)
        cuo.eye = ViewPos;
        cuo.up = ViewUp;
        cuo.forward = ViewForward;
        cuo.time = time;
    }

    void SetUniform(RHIUniform& InUniform) {
        SceneUniform = &InUniform;
    }

    virtual void Render(RenderViewport& Viewport, float4 ViewPos) {
        auto& Context = Viewport.Context;
        ImGUI.UpdateUI(pFuncImDraw);
        cuo.screenres.r = Viewport.WindowManager.GetWindowWidth();
        cuo.screenres.g = Viewport.WindowManager.GetWindowHeight();
        CameraUniform.CopyToBuffer(&Context, &cuo, sizeof(CameraUniformObject));

        GraphicDispatcher.WaitForGPUIdle(&Context);
        GraphicDispatcher.BeginFrame(&Context, &Viewport.WindowManager, &PresentPass);
        GraphicDispatcher.BeginRenderPass(&PresentPass);
        Pipeline.PipelineObject.SetUniform(SceneUniform, 0);
        Pipeline.PipelineObject.SetUniform(&CameraUniform, 1);
        GraphicDispatcher.BindIndexBuffer(&RHIFullScreenQuadIndexBuffer, 0);
        GraphicDispatcher.BindVertexBuffer(&RHIFullScreenQuadBuffer, 0, 0);
        IndexBufferSize = 6;
        GraphicDispatcher.Dispatch(&Pipeline.PipelineObject, IndexBufferSize, 0, 1);
        // Comment this line if you don't want ImGUI
        ImGUI.DispatchImGUI(&GraphicDispatcher);
        GraphicDispatcher.EndRenderPass(&PresentPass);
        GraphicDispatcher.EndFrameAndSubmit(&Context, &Viewport.WindowManager);
    }
};

void InitializeMeshRenderProxy(MeshRenderProxy& OutRenderProxy, Mesh& InMesh, RenderViewport& Viewport)
{
    OutRenderProxy.RHIVertexBuffer.Initialize(&Viewport.Context, sizeof(Mesh::VertexType), InMesh.Vertices.size(), BufferType::VERTEX);
    OutRenderProxy.RHIIndexBuffer.Initialize(&Viewport.Context, sizeof(Mesh::VertexType), InMesh.Vertices.size(), BufferType::INDEX);
    OutRenderProxy.RHIVertexBuffer.CopyToBuffer(&Viewport.Context, InMesh.Vertices.data(), InMesh.Vertices.size() * sizeof(Mesh::VertexType));
    OutRenderProxy.RHIIndexBuffer.CopyToBuffer(&Viewport.Context, InMesh.Indices.data(), InMesh.Indices.size() * sizeof(uint32_t));
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(InMesh.TexturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    assert(texHeight > 0 && texWidth > 0);
    OutRenderProxy.Texture.Initialize(&Viewport.Context, texHeight, texWidth, RHIFormat::R8G8B8A8_SRGB);
    OutRenderProxy.Texture.CopyToTexture(&Viewport.Context, pixels, 4);
    OutRenderProxy.IndexBufferSize = InMesh.Indices.size();
    stbi_image_free(pixels);
}

struct ModelUniformObject {
    alignas(16) float4x4 model;
    alignas(16) float4x4 modelInv;
    alignas(16) float3 color;
    alignas(16) float3 emission;
};

void SetModelUniform(RenderViewport& Viewport, RHIUniform& Uniform, float4x4 ModelMat, float3 Color) {
    ModelUniformObject uniformobject;
    Uniform.Initialize(&Viewport.Context, sizeof(ModelUniformObject));
    uniformobject.model = ModelMat;
    uniformobject.modelInv = glm::inverse(ModelMat);
    uniformobject.color = Color;
    Uniform.CopyToBuffer(&Viewport.Context, &uniformobject, sizeof(ModelUniformObject));
}

void SetSceneUniform(RenderViewport& Viewport, std::vector<ModelUniformObject>& Objects, float4x4 ModelMat, float3 Color, float3 emission) {
    ModelUniformObject uniformobject;
    uniformobject.model = ModelMat;
    uniformobject.modelInv = glm::inverse(ModelMat);
    uniformobject.color = Color;
    uniformobject.emission = emission;
    Objects.push_back(uniformobject);
}

int main() {
    GRHIImplementationSelection = RHIImplement_Vulkan;
    RenderViewport Viewport;
    Viewport.InitializeViewport(1000, 1000);
    PathTraceRenderer PTRenderer;
    PTRenderer.pFuncImDraw = DrawUI;
    PTRenderer.CreateRenderer(Viewport);

    ViewPos = float4(-0.27f, -0.8f, 0.27f, 1.f);
    ViewUp = float4(0., 0., 1., 0.);
    ViewForward = float4(0., 1., 0., 0.);

    RHIUniform SceneUniform;
    SceneUniform.Initialize(&Viewport.Context, sizeof(ModelUniformObject) * 8);

    std::vector<ModelUniformObject> Objects;
    // Floor
    SetSceneUniform(Viewport, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.27f, 0.001f)), float3(0.4f, 0.4f, 0.4f), float3(0., 0., 0.));

    // Celling
    SetSceneUniform(Viewport, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.54f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.27f, 0.001f)), float3(0.4f, 0.4f, 0.4f), float3(0., 0., 0.));

    // Leftwall
    SetSceneUniform(Viewport, Objects, glm::translate(float4x4(1.0), float3(-0.54f, 0.27f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.001f, 0.27f, 0.27f)), float3(0.5f, 0.f, 0.f), float3(0., 0., 0.));

    // Rightwall
    SetSceneUniform(Viewport, Objects, glm::translate(float4x4(1.0), float3(0.f, 0.27f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.001f, 0.27f, 0.27f)), float3(0.f, 0.5f, 0.f), float3(0., 0., 0.));

    // Backwall
    SetSceneUniform(Viewport, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.54f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.001f, 0.27f)), float3(0.4f, 0.4f, 0.4f), float3(0., 0., 0.));

    // Shortbox
    SetSceneUniform(Viewport, Objects, glm::translate(float4x4(1.0), float3(-0.185f, 0.169f, 0.0825f)) * glm::rotate(float4x4(1.0), glm::radians(-196.62f), float3(0, 0, 1.)) * glm::scale(float4x4(1.0f), float3(0.085f, 0.085f, 0.085f)), float3(0.4f, 0.4f, 0.4f), float3(0., 0., 0.));

    // Tallbox
    SetSceneUniform(Viewport, Objects, glm::translate(float4x4(1.0), float3(-0.368f, 0.351f, 0.165f)) * glm::rotate(float4x4(1.0), glm::radians(-252.77f), float3(0, 0, 1.)) * glm::scale(float4x4(1.0f), float3(0.085f, 0.085f, 0.17f)), float3(0.4f, 0.4f, 0.4f), float3(0., 0., 0.));

    // Light
    SetSceneUniform(Viewport, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.53f)) * glm::scale(float4x4(1.0f), float3(0.065f, 0.05f, 0.001f)), float3(1.f, 1.f, 1.f), float3(1., 1., 1.));

    SceneUniform.CopyToBuffer(&Viewport.Context, Objects.data(), Objects.size() * sizeof(ModelUniformObject));
    PTRenderer.SetUniform(SceneUniform);


    static auto startTime = std::chrono::high_resolution_clock::now();

    while (Viewport.IsWindowAlive())
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        PTRenderer.UpdateUniformBuffer(ViewPos, ViewForward, ViewUp, time);
        PTRenderer.Render(Viewport, ViewPos);
    }
    return 0;
}

int BlinnPhongMain()
{
    GRHIImplementationSelection = RHIImplement_Vulkan;
    Mesh StaticMesh = Mesh::LoadObj(CUBE_PATH);
    StaticMesh.TexturePath = TEXTURE_PATH;
    RenderViewport Viewport;
    Viewport.InitializeViewport(1000, 1000);
    BlinnPhongRenderer BPRenderer;
    BPRenderer.pFuncImDraw = DrawUI;
    BPRenderer.CreateRenderer(Viewport);

    MeshRenderProxy MeshProxy;
    InitializeMeshRenderProxy(MeshProxy, StaticMesh, Viewport);

    std::array<RHIUniform, 8> Uniforms;

    
    auto viewMat = glm::lookAt(glm::vec3(-0.27f, -0.8f, 0.27f), glm::vec3(-0.27f, 0.0f, 0.27f), glm::vec3(0.0f, 0.0f, 1.0f));
    ViewPos = float4(-0.27f, -0.8f, 0.27f, 1.f);
    ViewUp = float4(0., 0., 1., 0.);
    ViewForward = float4(0., 1., 0., 0.);
    
    
    // Floor
    SetModelUniform(Viewport, Uniforms[0], glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.27f, 0.001f)), float3(0.4f, 0.4f, 0.4f));

    // Celling
    SetModelUniform(Viewport, Uniforms[1], glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.54f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.27f, 0.001f)), float3(0.4f, 0.4f, 0.4f));

    // Leftwall
    SetModelUniform(Viewport, Uniforms[2], glm::translate(float4x4(1.0), float3(-0.54f, 0.27f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.f, 0.27f, 0.27f)), float3(0.5f, 0.f, 0.f));

    // Rightwall
    SetModelUniform(Viewport, Uniforms[3], glm::translate(float4x4(1.0), float3(0.f, 0.27f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.f, 0.27f, 0.27f)), float3(0.f, 0.5f, 0.f));

    // Backwall
    SetModelUniform(Viewport, Uniforms[4], glm::translate(float4x4(1.0), float3(-0.27f, 0.54f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.001f, 0.27f)), float3(0.4f, 0.4f, 0.4f));

    // Shortbox
    SetModelUniform(Viewport, Uniforms[5], glm::translate(float4x4(1.0), float3(-0.185f, 0.169f, 0.0825f)) * glm::rotate(float4x4(1.0), glm::radians(-196.62f), float3(0, 0, 1.)) * glm::scale(float4x4(1.0f), float3(0.085f, 0.085f, 0.085f)), float3(0.4f, 0.4f, 0.4f));

    // Tallbox
    SetModelUniform(Viewport, Uniforms[6], glm::translate(float4x4(1.0), float3(-0.368f, 0.351f, 0.165f)) * glm::rotate(float4x4(1.0), glm::radians(-252.77f), float3(0, 0, 1.)) * glm::scale(float4x4(1.0f), float3(0.085f, 0.085f, 0.17f)), float3(0.4f, 0.4f, 0.4f));

    // Light
    SetModelUniform(Viewport, Uniforms[7], glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.53f)) * glm::scale(float4x4(1.0f), float3(0.065f, 0.05f, 0.001f)), float3(0.4f, 0.4f, 0.4f));

    for (auto& Uniform : Uniforms) {
        BPRenderer.DrawMesh(MeshProxy, Uniform);
    }

    while (Viewport.IsWindowAlive())
    {
        BPRenderer.Render(Viewport, viewMat, ViewPos);
    }
    return 0;
}


int PBRRendererTest()
{
    GRHIImplementationSelection = RHIImplement_Vulkan;
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