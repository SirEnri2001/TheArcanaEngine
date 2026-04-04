#define COREGEOMETRY_INCLUDE
#define CORESCENE_INCLUDE
#define RENDERER_INCLUDE
#define SHADERCOMPILER_INCLUDE
#define PBR_RENDERER_INCLUDE
#define RHI_INCLUDE

#include <chrono>
#include <fstream>

#include "CoreMath.h"
#include "CoreMath.inl"
#include "CoreGeometry.h"
#include "CoreScene.h"
#include "CoreLog.inl"
//#include "PBRRenderer.h"
//#include "Renderer.h"
#include "RHI.h"
#include "RHIImGuiHelper.h"
#include "ShaderCompiler.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const std::string MODEL_PATH = "models/spot/spot_triangulated_good.obj";
const std::string TEXTURE_PATH = "models/spot/spot_texture.png";
const std::string MODEL2_PATH = "models/viking_room.obj";
const std::string TEXTURE2_PATH = "textures/viking_room.png";
const std::string VERT_SHADER_PATH = "shaderbytecode/glsl/BlinnPhong.vert";
const std::string FRAG_SHADER_PATH = "shaderbytecode/glsl/BlinnPhong.frag";
const std::string CUBE_PATH = "models/cube/cube.obj";

bool Recompile = false;
bool TestShaders = false;
bool RenderingPaused = false;
bool ShaderCompileHasError = false;


void CompileAllShaders(bool CompileTest = false);

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


void readFile_U32I(const std::string& filename, std::vector<uint32_t>& buffer_u32) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    buffer_u32.resize(buffer.size()/4);
    for (int i = 0; i < buffer.size(); i+=4) {
        // Assuming little-endian file format; adjust if needed
        buffer_u32[i / 4] = static_cast<uint32_t>(buffer[i]) |
            (static_cast<uint32_t>(buffer[i + 1]) << 8) |
            (static_cast<uint32_t>(buffer[i + 2]) << 16) |
            (static_cast<uint32_t>(buffer[i + 3]) << 24);
    }
}

template<class T>
std::vector<T> read_spirv_file(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        fprintf(stderr, "Failed to open SPIR-V file: %s\n", path);
        return {};
    }

    fseek(file, 0, SEEK_END);
    long len = ftell(file) / sizeof(T);
    rewind(file);

    std::vector<T> spirv(len);
    if (fread(spirv.data(), sizeof(T), len, file) != size_t(len))
        spirv.clear();

    fclose(file);
    return spirv;
}

//float4 ViewPos = { 2.,2.,2.,1. };
//float4 ViewForward;
//float4 ViewUp;
float4x4 CameraTransformLocalToWorld;
bool isClick = false;
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

        ImGui::Begin("Toolbar");                          // Create a window called "Hello, world!" and append into it.
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state

        if (ImGui::Button("Recompile Shaders")) {
            Recompile = true;
            TestShaders = false;
        }
        
        if (ImGui::Button("Recompile Test Shaders")) {
            Recompile = true;
            TestShaders = true;
        }

        if (ImGui::Button("Pause")) {
            RenderingPaused = true;
            ImGui::OpenPopup("Pause Rendering");
        }

        if (ShaderCompileHasError) {
            ImGui::OpenPopup("Shader Compiler Error");
        }

        // Always center this window when appearing
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Pause Rendering", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Rendering paused. ");
            ImGui::Separator();

            if (ImGui::Button("Continue", ImVec2(120, 0)))
            {
                RenderingPaused = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Shader Compiler Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Shader has compilation error, check console for more information. ");
            ImGui::Separator();

            if (ImGui::Button("Retry", ImVec2(120, 0))) 
            { 
                CompileAllShaders(TestShaders);
                ImGui::CloseCurrentPopup(); 
            }
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }

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
        isClick = true;
        float DeltaX = io.MousePos.x - io.MousePosPrev.x;
        float DeltaY = io.MousePos.y - io.MousePosPrev.y;

        DeltaX *= 0.001;
        DeltaY *= 0.001;

        float3 PlayerMove = { 0.f, 0.f, 0.f };
        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            PlayerMove += float3(1.f, 0.f, 0.f);
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            PlayerMove -= float3(1.f, 0.f, 0.f);
        }
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            PlayerMove += float3(0.f, 1.f, 0.f);
        }
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            PlayerMove -= float3(0.f, 1.f, 0.f);
        }
        if (ImGui::IsKeyDown(ImGuiKey_E) || ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
        {
            PlayerMove -= float3(0.f, 0.f, 1.f);
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q) || ImGui::IsKeyDown(ImGuiKey_Space))
        {
            PlayerMove += float3(0.f, 0.f, 1.f);
        }
        float4x4 ViewTransform = float4x4(1.0f);
        float4x4 PitchRotate = glm::rotate(float4x4(1.0f), DeltaY, float3(0.f, 1.f, 0.f));
        float4x4 YawRotate = glm::rotate(float4x4(1.0f), DeltaX, float3(0.f, 0.f, 1.f));
        float4x4 Translate = glm::translate(float4x4(1.0f), PlayerMove*0.001f);
        CameraTransformLocalToWorld = CameraTransformLocalToWorld * Translate * PitchRotate * YawRotate;
        //ViewPos += float4(PlayerMove*0.01f, 0.0f);
        //ViewForward.z += DeltaY;
        //ViewForward.x += -DeltaX;
        //ViewForward = glm::normalize(ViewForward);
        //ViewUp = float4(0., 0., 1., 0.);
        //float3 ViewLeft = glm::normalize(glm::cross(float3(ViewUp), float3(ViewForward)));
        //ViewUp = float4(glm::normalize(glm::cross(float3(ViewForward), float3(ViewLeft))), 0.0f);
        //ViewForward = float4(glm::normalize(glm::cross(float3(ViewUp), glm::cross(float3(ViewForward), float3(ViewUp)))), 0.0);
    }else
    {
        isClick = false;
    }
}

int PBRRendererTest();

class PassPipeline {
public:
    virtual void InitializePipeline(IRHIContext* Context, IRHIRenderPass& RenderPass) = 0;
    virtual void ReloadPipeline(IRHIContext* Context, IRHIRenderPass& RenderPass) {}
};

typedef char ShaderFileType;

class ComputePipeline
{
public:
    std::unique_ptr<IRHIPipelineFactory> PipelineFactory;
    std::unique_ptr<IRHIPipelineObject> PipelineObject;

    std::vector<ShaderFileType> ComputeShaderSPIRV;

    ComputePipeline() {
        ComputeShaderSPIRV = readFile("shaderbytecode/glsl/Test.comp.spv");
    }

    virtual void InitializePipeline(IRHIContext* Context) {
        PipelineFactory = Context->CreateRHIPipelineFactory();
        PipelineObject = Context->CreateRHIPipelineObject();
        PipelineFactory->SetComputeShaders(ComputeShaderSPIRV);
        PipelineFactory->SetDescriptorBinding(0, DescriptorType::IMAGE2D);
        PipelineFactory->SetDescriptorBinding(1, DescriptorType::STORAGE_READONLY);
        PipelineFactory->SetDescriptorBinding(2, DescriptorType::STORAGE_READONLY);
        //PipelineFactory->SetStorageBufferBinding(1);
        //PipelineFactory->SetStorageBufferBinding(2);
        PipelineFactory->InitializeComputePipelineObject(PipelineObject.get(), Context);
    }

    virtual void ReloadPipeline(IRHIContext* Context)  {
        ComputeShaderSPIRV = readFile("shaderbytecode/glsl/Test.comp.spv");
        PipelineFactory->SetComputeShaders(ComputeShaderSPIRV);
        PipelineObject->Cleanup(Context);
        PipelineFactory->InitializeComputePipelineObject(PipelineObject.get(), Context);
    }
};

class PathTracerPipeline : public PassPipeline {
public:
    std::unique_ptr<IRHIPipelineFactory> PipelineFactory;
    std::unique_ptr<IRHIPipelineObject> PipelineObject;

    std::vector<ShaderFileType> VertexShaderSPIRV;
    std::vector<ShaderFileType> FragmentShaderSPIRV;

    PathTracerPipeline() {
        VertexShaderSPIRV = readFile("shaderbytecode/glsl/PathTracer.vert.spv");
        FragmentShaderSPIRV = readFile("shaderbytecode/glsl/PathTracer.frag.spv");
    }

    virtual void InitializePipeline(IRHIContext* Context, IRHIRenderPass& RenderPass) override {
        PipelineFactory = Context->CreateRHIPipelineFactory();
        PipelineObject = Context->CreateRHIPipelineObject();
        PipelineFactory->SetShaders(VertexShaderSPIRV, FragmentShaderSPIRV);
        PipelineFactory->SetDescriptorBinding(0, DescriptorType::UNIFORM);
        PipelineFactory->SetDescriptorBinding(1, DescriptorType::UNIFORM);
        PipelineFactory->SetDescriptorBinding(2, DescriptorType::SAMPLER2D);
        PipelineFactory->AddBufferBinding(0, sizeof(float3));
        PipelineFactory->AddBufferLayout(0, 0, R32G32B32_SFLOAT, 0);
        PipelineFactory->InitializePipelineObject(PipelineObject.get(), Context, &RenderPass);
    }

    virtual void ReloadPipeline(IRHIContext* Context, IRHIRenderPass& RenderPass) override {
        VertexShaderSPIRV = readFile("shaderbytecode/glsl/PathTracer.vert.spv");
        FragmentShaderSPIRV = readFile("shaderbytecode/glsl/PathTracer.frag.spv");
        PipelineFactory->SetShaders(VertexShaderSPIRV, FragmentShaderSPIRV);
        PipelineObject->Cleanup(Context);
        PipelineFactory->InitializePipelineObject(PipelineObject.get(), Context, &RenderPass);
    }
};

class PTPostPipeline : public PassPipeline {
public:
    std::unique_ptr<IRHIPipelineFactory> PipelineFactory;
    std::unique_ptr<IRHIPipelineObject> PipelineObject;

    std::vector<ShaderFileType> VertexShaderSPIRV;
    std::vector<ShaderFileType> FragmentShaderSPIRV;
    PTPostPipeline() {
        VertexShaderSPIRV = readFile("shaderbytecode/glsl/ScreenPost.vert.spv");
        FragmentShaderSPIRV = readFile("shaderbytecode/glsl/ScreenPost.frag.spv");
    }

    virtual void InitializePipeline(IRHIContext* Context, IRHIRenderPass& RenderPass) override {
        PipelineFactory = Context->CreateRHIPipelineFactory();
        PipelineObject = Context->CreateRHIPipelineObject();
        PipelineFactory->SetShaders(VertexShaderSPIRV, FragmentShaderSPIRV);
        //PipelineFactory->SetImageSamplerBinding(0);
        PipelineFactory->SetDescriptorBinding(0, DescriptorType::SAMPLER2D);
        PipelineFactory->SetDescriptorBinding(1, DescriptorType::UNIFORM);
        //PipelineFactory->SetUniformBinding(1);
        PipelineFactory->AddBufferBinding(0, sizeof(float3));
        PipelineFactory->AddBufferLayout(0, 0, R32G32B32_SFLOAT, 0);
        PipelineFactory->InitializePipelineObject(PipelineObject.get(), Context, &RenderPass);
    }

    virtual void ReloadPipeline(IRHIContext* Context, IRHIRenderPass& RenderPass) override {
        VertexShaderSPIRV = readFile("shaderbytecode/glsl/ScreenPost.vert.spv");
        FragmentShaderSPIRV = readFile("shaderbytecode/glsl/ScreenPost.frag.spv");
        PipelineFactory->SetShaders(VertexShaderSPIRV, FragmentShaderSPIRV);
        PipelineObject->Cleanup(Context);
        PipelineFactory->InitializePipelineObject(PipelineObject.get(), Context, &RenderPass);
    }
};

class PathTraceRenderer {
public:
    uint32_t IndexBufferSize;
    std::unique_ptr<IRHIContext> uptr_Context;
    IRHIContext* Context;
    std::unique_ptr<IRHIBuffer> RHIFullScreenQuadBuffer;
    std::unique_ptr<IRHIBuffer> RHIFullScreenQuadIndexBuffer;
    std::unique_ptr<IRHIImageResource> RHIScreenBuffer1;
    std::unique_ptr<IRHIImageResource> RHIScreenBuffer2;
    std::unique_ptr<IRHIImageResource> RHIScreenBufferDepth;
    std::unique_ptr<IRHIImageResource> RHIStoreImage;
    std::unique_ptr<IRHIRenderPass> PresentPass;
    std::unique_ptr<IRHIRenderPass> PTRenderPass;
    std::unique_ptr<IRHIBuffer> CameraUniform;
    std::unique_ptr<IRHIBuffer> SceneUniform;
    std::unique_ptr<IRHIBuffer> StorageBuffer;
    std::unique_ptr<IRHIBuffer> PrimitiveBuffer;
    std::unique_ptr<IRHISwapchain> Swapchain;
    std::unique_ptr<IRHIFrameBuffer> FBuffer1;
    std::unique_ptr<IRHIFrameBuffer> FBuffer2;
    std::unique_ptr<IRHIImGUI> ImGUI;

    PathTracerPipeline Pipeline;
    PTPostPipeline PostPipeline;
    ComputePipeline TestCompPipeline;

    bool swap = false;

    void (*pFuncImDraw)(ImGuiSharedGlobals*);

    struct CameraUniformObject {
        alignas(16) float4x4 camToWorld;
        alignas(8) glm::uvec2 screenres; // not sure 8 or 16 to use here, renderdoc says shader uses 8
        alignas(8) float time;
        alignas(4) int frameId;
    } cuo;

    PathTraceRenderer() = default;

    virtual void CreateRenderer(uint32_t Height, uint32_t Width) {
        uptr_Context = IRHIPlatformSupport::Get(RHIBackend::D3D12)->CreateRHIContext();
        Context = uptr_Context.get();
        Context->Initialize(Width, Height);
        RHIFullScreenQuadBuffer = Context->CreateRHIBuffer();
        RHIFullScreenQuadIndexBuffer = Context->CreateRHIBuffer();
        RHIScreenBuffer1 = Context->CreateRHIImageResource();
        RHIScreenBuffer2 = Context->CreateRHIImageResource();
        RHIScreenBufferDepth = Context->CreateRHIImageResource();
        RHIStoreImage = Context->CreateRHIImageResource();
        PresentPass = Context->CreateRHIRenderPass();
        PTRenderPass = Context->CreateRHIRenderPass();
        CameraUniform = Context->CreateRHIBuffer();
        SceneUniform = Context->CreateRHIBuffer();
        Swapchain = Context->CreateRHISwapchain();
        FBuffer1 = Context->CreateRHIFrameBuffer();
        FBuffer2 = Context->CreateRHIFrameBuffer();
        ImGUI = Context->CreateRHIImGUI();
        StorageBuffer = Context->CreateRHIBuffer();
        PrimitiveBuffer = Context->CreateRHIBuffer();

        // create renderer
        Swapchain->Initialize(Context);
        ImageExtent3D ext = Swapchain->GetFrameSize();
        RHIScreenBuffer1->Initialize(Context, ext, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::COLOR_ATTACHMENT | RHIResourceState::SHADER_READ, 1);
        RHIScreenBuffer2->Initialize(Context, ext, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::COLOR_ATTACHMENT | RHIResourceState::SHADER_READ, 1);
        RHIScreenBufferDepth->Initialize(Context, ext, RHIFormat::D32_SFLOAT, RHIResourceState::DEPTH_ATTACHMENT, 1);
        RHIStoreImage->Initialize(Context, ext, RHIFormat::R32G32B32A32_SFLOAT, RHIResourceState::SHADER_WRITE | RHIResourceState::SHADER_READ, 1);

    	std::vector<RHIFormat> ColorRTFormats = { RHIFormat::R8G8B8A8_SRGB };
        //std::vector<RHIFormat> ColorRTFormats1 = { RHIFormat::B8G8R8A8_SRGB };
        std::vector<RHIFormat> ColorRTFormats1 = { RHIFormat::R8G8B8A8_UNORM };
        PTRenderPass->Initialize(Context, ColorRTFormats);
        PresentPass->Initialize(Context, ColorRTFormats1);

        Pipeline.InitializePipeline(Context, *PTRenderPass);
        PostPipeline.InitializePipeline(Context, *PresentPass);
        TestCompPipeline.InitializePipeline(Context);

        RHIFullScreenQuadBuffer->Initialize(Context, sizeof(float3)*8, RHIResourceState::BUFFER_VERTEX);
        RHIFullScreenQuadIndexBuffer->Initialize(Context, sizeof(uint32_t)* 6, RHIResourceState::BUFFER_INDEX);
        float3 FullScreenVertices[4] = { float3(-1., -1., 0.), float3(-1., 1., 0.), float3(1., 1., 0.), float3(1., -1., 0.) };
        uint32_t FullScreenVerticesIndex[6] = { 0, 1, 2, 0, 2, 3 };

        //std::unique_ptr<IRHICommandBuffer> CmdBuffer = Context->CreateRHICommandBuffer();
        //CmdBuffer->Initialize(Context);
        RHIFullScreenQuadBuffer->CopyToBuffer(Context, FullScreenVertices, sizeof(float3) * 8);
        RHIFullScreenQuadIndexBuffer->CopyToBuffer(Context, FullScreenVerticesIndex, sizeof(uint32_t) * 6);
        CameraUniform->Initialize(Context, sizeof(CameraUniformObject), RHIResourceState::BUFFER_UNIFORM);
        StorageBuffer->Initialize(Context, sizeof(CameraUniformObject), RHIResourceState::BUFFER_SHADER_STORAGE);
        ImGUI->Initialize(Context, Swapchain.get(), PresentPass.get());
        std::vector<IRHIImageResource*> ColorRT1 = { RHIScreenBuffer1.get() };
        std::vector<IRHIImageResource*> ColorRT2 = { RHIScreenBuffer2.get() };
        FBuffer1->Initialize(Context, PTRenderPass.get(), ColorRT1, RHIScreenBufferDepth.get());
        FBuffer2->Initialize(Context, PTRenderPass.get(), ColorRT2, RHIScreenBufferDepth.get());

        cuo.frameId = 1;
    }

    void UpdateUniformBuffer(float4x4 camToWorld, float time, bool isClick) {
        // sensor size 32mm
        // focal length 45mm
        // fov = 2 * atan(sensor_size/2/focal_length)
        cuo.camToWorld = camToWorld;
        cuo.time = time;
        if (isClick)
        {
            cuo.frameId = 1;
        }
    }

    virtual void Render(float4 ViewPos) {
        ImGUI->UpdateUI(pFuncImDraw);
        cuo.screenres.r = Swapchain->GetFrameSize().Width;
        cuo.screenres.g = Swapchain->GetFrameSize().Height;
        cuo.frameId++;
        CameraUniform->CopyToBuffer(Context, &cuo, sizeof(CameraUniformObject));
        StorageBuffer->CopyToBuffer(Context, &cuo, sizeof(CameraUniformObject));
        IRHIFrameBuffer* FrameBuffer = nullptr;
        Swapchain->AcquireFrame(Context, FrameBuffer, PresentPass.get());
        std::unique_ptr<IRHICommandBuffer> CommandBuffer;
        CommandBuffer = Context->CreateRHICommandBuffer();
        CommandBuffer->Initialize(Context);
        CommandBuffer->BeginCommandBuffer();
        if (Recompile) {
            CompileAllShaders(TestShaders);
            Pipeline.ReloadPipeline(Context, *PTRenderPass);
            PostPipeline.ReloadPipeline(Context, *PresentPass);
            TestCompPipeline.ReloadPipeline(Context);
            Recompile = false;
        }

        RHIStoreImage->Transition(CommandBuffer.get(), RHIResourceState::SHADER_WRITE);
        TestCompPipeline.PipelineObject->SetBindingResource(0, DescriptorType::IMAGE2D, RHIStoreImage.get());
        TestCompPipeline.PipelineObject->SetStorageBuffer(StorageBuffer.get(), 1);
        TestCompPipeline.PipelineObject->SetStorageBuffer(PrimitiveBuffer.get(), 2);
    	TestCompPipeline.PipelineObject->Dispatch(CommandBuffer.get(), 64, 64, 1);
        //if (swap) {
        //    RHIScreenBuffer2->Transition(CommandBuffer.get(), RHIResourceState::COLOR_ATTACHMENT);
        //    RHIScreenBuffer1->Transition(CommandBuffer.get(), RHIResourceState::SHADER_READ);
        //    PTRenderPass->BeginRenderPass(CommandBuffer.get(), FBuffer2.get());
        //}
        //else {
        //    RHIScreenBuffer1->Transition(CommandBuffer.get(), RHIResourceState::COLOR_ATTACHMENT);
        //    RHIScreenBuffer2->Transition(CommandBuffer.get(), RHIResourceState::SHADER_READ);
        //    PTRenderPass->BeginRenderPass(CommandBuffer.get(), FBuffer1.get());
        //}
        //if (!RenderingPaused && !ShaderCompileHasError) {
        //    Pipeline.PipelineObject->SetUniform(SceneUniform.get(), 0);
        //    Pipeline.PipelineObject->SetUniform(CameraUniform.get(), 1);
        //    Pipeline.PipelineObject->SetImageSampler(swap ? RHIScreenBuffer1.get() : RHIScreenBuffer2.get(), 2);
        //    Pipeline.PipelineObject->BindIndexBuffer(RHIFullScreenQuadIndexBuffer.get(), 0);
        //    Pipeline.PipelineObject->BindVertexBuffer(RHIFullScreenQuadBuffer.get(), 0, 0);
        //    IndexBufferSize = 6;
        //    Pipeline.PipelineObject->Draw(CommandBuffer.get(), IndexBufferSize, 0, 1);
        //}
        //PTRenderPass->EndRenderPass(CommandBuffer.get());
        RHIStoreImage->Transition(CommandBuffer.get(), RHIResourceState::SHADER_READ);
        PresentPass->BeginRenderPass(CommandBuffer.get(), FrameBuffer);
        // Comment this line if you don't want ImGUI
        if (!RenderingPaused && !ShaderCompileHasError) {
            PostPipeline.PipelineObject->SetUniform(CameraUniform.get(), 1);
            PostPipeline.PipelineObject->SetImageSampler(RHIStoreImage.get(), 0);
            PostPipeline.PipelineObject->BindIndexBuffer(RHIFullScreenQuadIndexBuffer.get(), 0);
            PostPipeline.PipelineObject->BindVertexBuffer(RHIFullScreenQuadBuffer.get(), 0, 0);
            IndexBufferSize = 6;
            PostPipeline.PipelineObject->Draw(CommandBuffer.get(), IndexBufferSize, 0, 1);
        }
        ImGUI->DispatchImGUI(CommandBuffer.get());
        PresentPass->EndRenderPass(CommandBuffer.get());
        CommandBuffer->EndCommandBuffer();
        TestCompPipeline.PipelineObject->CopyDescriptors(Context);
        PostPipeline.PipelineObject->CopyDescriptors(Context);
        Swapchain->PresentFrameAndRelease(Context, CommandBuffer.get());
        Context->WaitDeviceIdle();
        Context->ProcessFrameInput();
        swap  = !swap;
    }
};

//void InitializeMeshRenderProxy(MeshRenderProxy& OutRenderProxy, Mesh& InMesh, RenderViewport& Viewport)
//{
//    IRHICommandBuffer CmdBuffer;
//    CmdBuffer.Initialize(&Viewport.Context);
//    OutRenderProxy.RHIVertexBuffer.Initialize(&Viewport.Context, sizeof(Mesh::VertexType), InMesh.Vertices.size(), BufferType::VERTEX);
//    OutRenderProxy.RHIIndexBuffer.Initialize(&Viewport.Context, sizeof(Mesh::VertexType), InMesh.Vertices.size(), BufferType::INDEX);
//    OutRenderProxy.RHIVertexBuffer.CopyToBuffer(&CmdBuffer, &Viewport.Context, InMesh.Vertices.data(), InMesh.Vertices.size() * sizeof(Mesh::VertexType));
//    OutRenderProxy.RHIIndexBuffer.CopyToBuffer(&CmdBuffer, &Viewport.Context, InMesh.Indices.data(), InMesh.Indices.size() * sizeof(uint32_t));
//    int texWidth, texHeight, texChannels;
//    stbi_uc* pixels = stbi_load(InMesh.TexturePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
//    assert(texHeight > 0 && texWidth > 0);
//    OutRenderProxy.Texture.Initialize(&Viewport.Context, texHeight, texWidth, RHIFormat::R8G8B8A8_SRGB, RHIResourceState::SHADER_READ);
//    OutRenderProxy.Texture.CopyToTexture(&CmdBuffer, &Viewport.Context, pixels, 4);
//    OutRenderProxy.IndexBufferSize = InMesh.Indices.size();
//    stbi_image_free(pixels);
//}

struct ModelUniformObject {
    alignas(16) float4x4 model;
    alignas(16) float4x4 modelInv;
    alignas(16) float4x4 modelInvTranspose;
    alignas(16) float3 color;
    alignas(16) float3 emission;
};

void SetModelUniform(IRHIContext* Context, IRHIBuffer& Uniform, float4x4 ModelMat, float3 Color) {
    ModelUniformObject uniformobject;
    Uniform.Initialize(Context, sizeof(ModelUniformObject), RHIResourceState::BUFFER_UNIFORM);
    uniformobject.model = ModelMat;
    uniformobject.modelInv = glm::inverse(ModelMat);
    uniformobject.color = Color;
    uniformobject.modelInvTranspose = glm::transpose(uniformobject.modelInv);
    Uniform.CopyToBuffer(Context, &uniformobject, sizeof(ModelUniformObject));
}

void SetSceneUniform(IRHIContext* Context, std::vector<ModelUniformObject>& Objects, float4x4 ModelMat, float3 Color, float3 emission) {
    ModelUniformObject uniformobject;
    uniformobject.model = ModelMat;
    uniformobject.modelInv = glm::inverse(ModelMat);
    uniformobject.modelInvTranspose = glm::transpose(uniformobject.modelInv);
    uniformobject.color = Color;
    uniformobject.emission = emission;
    Objects.push_back(uniformobject);
}

#define PT_VERTSHADER "./shaders/glsl/PathTracer.vert"
#define PT_FRAGSHADER "./shaders/glsl/PathTracer.frag"
#define PTPOST_VERTSHADER "./shaders/glsl/ScreenPost.vert"
#define PTPOST_FRAGSHADER "./shaders/glsl/ScreenPost.frag"
#define TEST_COMPSHADER "./shaders/glsl/Test.comp"

void CompileAllShaders(bool CompileTest) {
    GLSLCompiler Compiler;
    bool IsSucceeded = true;
    IsSucceeded = true;
    if (CompileTest)
    {
        IsSucceeded = IsSucceeded && Compiler.DirectCompile(PT_VERTSHADER, "./shaderbytecode/glsl/PathTracer.vert.spv", "-DCOMPILETEST");
        IsSucceeded = IsSucceeded && Compiler.DirectCompile(PT_FRAGSHADER, "./shaderbytecode/glsl/PathTracer.frag.spv", "-DCOMPILETEST");
        IsSucceeded = IsSucceeded && Compiler.DirectCompile(PTPOST_VERTSHADER, "./shaderbytecode/glsl/ScreenPost.vert.spv", "-DCOMPILETEST");
        IsSucceeded = IsSucceeded && Compiler.DirectCompile(PTPOST_FRAGSHADER, "./shaderbytecode/glsl/ScreenPost.frag.spv", "-DCOMPILETEST");
        IsSucceeded = IsSucceeded && Compiler.DirectCompile(TEST_COMPSHADER, "./shaderbytecode/glsl/Test.comp.spv", "-DCOMPILETEST");
    }
    else {
        IsSucceeded = IsSucceeded && Compiler.DirectCompile(PT_VERTSHADER, "./shaderbytecode/glsl/PathTracer.vert.spv", "");
        IsSucceeded = IsSucceeded && Compiler.DirectCompile(PT_FRAGSHADER, "./shaderbytecode/glsl/PathTracer.frag.spv", "");
        IsSucceeded = IsSucceeded && Compiler.DirectCompile(PTPOST_VERTSHADER, "./shaderbytecode/glsl/ScreenPost.vert.spv", "");
        IsSucceeded = IsSucceeded && Compiler.DirectCompile(PTPOST_FRAGSHADER, "./shaderbytecode/glsl/ScreenPost.frag.spv", "");
        IsSucceeded = IsSucceeded && Compiler.DirectCompile(TEST_COMPSHADER, "./shaderbytecode/glsl/Test.comp.spv", "");
    }
    if (!IsSucceeded) {
        ShaderCompileHasError = true;
    }
    else {
        ShaderCompileHasError = false;
    }
}

#if 1
int main() {
	do {
        CompileAllShaders();
        if (ShaderCompileHasError) {
            system("pause");
        }
    } while (ShaderCompileHasError);
    Log("All shaders compilation succeeded. ");
    PathTraceRenderer PTRenderer;
    PTRenderer.pFuncImDraw = DrawUI;
    PTRenderer.CreateRenderer(1000, 1000);
    CameraTransformLocalToWorld = float4x4(1.f);
    CameraTransformLocalToWorld[0] = float4(0.f, 1.f, 0.f, 0.f);
    CameraTransformLocalToWorld[1] = float4(-1.f, 0.f, 0.f, 0.f);
    CameraTransformLocalToWorld[3] = float4(-0.27f, -0.8f, 0.27f, 1.f);

    PTRenderer.SceneUniform->Initialize(PTRenderer.Context, sizeof(ModelUniformObject), 8, RHIResourceState::BUFFER_UNIFORM);

    std::vector<ModelUniformObject> Objects;
    // Floor
    SetSceneUniform(PTRenderer.Context, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.27f, 0.001f)), float3(0.4f, 0.4f, 0.4f), float3(0., 0., 0.));

    // Celling
    SetSceneUniform(PTRenderer.Context, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.54f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.27f, 0.001f)), float3(0.4f, 0.4f, 0.4f), float3(0., 0., 0.));

    // Leftwall
    SetSceneUniform(PTRenderer.Context, Objects, glm::translate(float4x4(1.0), float3(-0.54f, 0.27f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.001f, 0.27f, 0.27f)), float3(0.5f, 0.f, 0.f), float3(0., 0., 0.));

    // Rightwall
    SetSceneUniform(PTRenderer.Context, Objects, glm::translate(float4x4(1.0), float3(0.f, 0.27f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.001f, 0.27f, 0.27f)), float3(0.f, 0.5f, 0.f), float3(0., 0., 0.));

    // Backwall
    SetSceneUniform(PTRenderer.Context, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.54f, 0.27f)) * glm::scale(float4x4(1.0f), float3(0.27f, 0.001f, 0.27f)), float3(0.4f, 0.4f, 0.4f), float3(0., 0., 0.));

    // Shortbox
    SetSceneUniform(PTRenderer.Context, Objects, glm::translate(float4x4(1.0), float3(-0.185f, 0.169f, 0.0825f)) * glm::rotate(float4x4(1.0), glm::radians(-196.62f), float3(0, 0, 1.)) * glm::scale(float4x4(1.0f), float3(0.085f, 0.085f, 0.085f)), float3(0.4f, 0.4f, 0.4f), float3(0., 0., 0.));

    // Tallbox
    SetSceneUniform(PTRenderer.Context, Objects, glm::translate(float4x4(1.0), float3(-0.368f, 0.351f, 0.165f)) * glm::rotate(float4x4(1.0), glm::radians(-252.77f), float3(0, 0, 1.)) * glm::scale(float4x4(1.0f), float3(0.085f, 0.085f, 0.17f)), float3(0.4f, 0.4f, 0.4f), float3(0., 0., 0.));

    // Light
    SetSceneUniform(PTRenderer.Context, Objects, glm::translate(float4x4(1.0), float3(-0.27f, 0.27f, 0.53999f)) * glm::scale(float4x4(1.0f), float3(0.065f, 0.05f, 0.001f)), float3(1.f, 1.f, 1.f), float3(1., 1., 1.)*200.f);

    PTRenderer.SceneUniform->CopyToBuffer(PTRenderer.Context, Objects.data(), Objects.size() * sizeof(ModelUniformObject));

    PTRenderer.PrimitiveBuffer->Initialize(PTRenderer.Context, sizeof(ModelUniformObject) * 8, RHIResourceState::BUFFER_SHADER_STORAGE);
    PTRenderer.PrimitiveBuffer->CopyToBuffer(PTRenderer.Context, Objects.data(), Objects.size() * sizeof(ModelUniformObject));

    static auto startTime = std::chrono::high_resolution_clock::now();

    while (PTRenderer.Context->IsWindowAlive())
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        PTRenderer.UpdateUniformBuffer(CameraTransformLocalToWorld, time, isClick);
        PTRenderer.Render(float4(1.f));
    }
    return 0;
}
#elif 1

class BlinnPhongPipeline : public PassPipeline {
public:
    std::unique_ptr<IRHIPipelineFactory> PipelineFactory;
    std::unique_ptr<IRHIPipelineObject> PipelineObject;

    std::vector<ShaderFileType> VertexShaderSPIRV;
    std::vector<ShaderFileType> FragmentShaderSPIRV;

    BlinnPhongPipeline() {
        VertexShaderSPIRV = readFile("shaderbytecode/glsl/BlinnPhong.vert.spv");
        FragmentShaderSPIRV = readFile("shaderbytecode/glsl/BlinnPhong.frag.spv");
    }

    virtual void InitializePipeline(IRHIContext* Context, IRHIRenderPass& RenderPass) override {
        PipelineFactory = Context->CreateRHIPipelineFactory();
        PipelineObject = Context->CreateRHIPipelineObject();
        PipelineFactory->SetShaders(VertexShaderSPIRV, FragmentShaderSPIRV);
        PipelineFactory->SetUniformBinding(0);
        PipelineFactory->SetUniformBinding(1);
        PipelineFactory->SetImageSamplerBinding(2);
        PipelineFactory->AddBufferBinding(0, sizeof(float3));
        PipelineFactory->AddBufferLayout(0, 0, R32G32B32_SFLOAT, 0);
        PipelineFactory->InitializePipelineObject(PipelineObject.get(), Context, &RenderPass);
    }

    virtual void ReloadPipeline(IRHIContext* Context, IRHIRenderPass& RenderPass) override {
        VertexShaderSPIRV = readFile("shaderbytecode/glsl/BlinnPhong.vert.spv");
        FragmentShaderSPIRV = readFile("shaderbytecode/glsl/BlinnPhong.frag.spv");
        PipelineFactory->SetShaders(VertexShaderSPIRV, FragmentShaderSPIRV);
        PipelineObject->Cleanup(Context);
        PipelineFactory->InitializePipelineObject(PipelineObject.get(), Context, &RenderPass);
    }
};
#elif 0

#include "spirv_glsl.hpp"
#include <vector>
#include <utility>

int main()
{
    //readFile("shaderbytecode/glsl/PathTracer.frag");
    // Read SPIR-V from disk or similar.
    
    std::vector<char> spirv_binary = readFile("shaderbytecode/glsl/PathTracer.vert");
    //std::vector<char> spirv_binary = read_spirv_file<char>("shaderbytecode/glsl/PathTracer.vert");

    spirv_cross::CompilerGLSL glsl(reinterpret_cast<const uint32_t*>(spirv_binary.data()), spirv_binary.size()/4);

    // The SPIR-V is now parsed, and we can perform reflection on it.
    spirv_cross::ShaderResources resources = glsl.get_shader_resources();

    // Get all sampled images in the shader.
    for (auto& resource : resources.sampled_images)
    {
        unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
        unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
        printf("Image %s at set = %u, binding = %u\n", resource.name.c_str(), set, binding);

        // Modify the decoration to prepare it for GLSL.
        glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);

        // Some arbitrary remapping if we want.
        glsl.set_decoration(resource.id, spv::DecorationBinding, set * 16 + binding);
    }

    // Set some options.
    spirv_cross::CompilerGLSL::Options options;
    options.version = 310;
    options.es = true;
    glsl.set_common_options(options);

    // Compile to GLSL, ready to give to GL driver.
    std::string source = glsl.compile();
    std::cout << source;
}

int PBRRendererTest()
{
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
#endif