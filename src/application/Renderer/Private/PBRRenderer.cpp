#define PBR_RENDERER_IMPLEMENT
#define RENDERER_INCLUDE

#include "PBRRenderer.h"
#include <CoreLog.inl>
#include <imgui.h>
#include <RHIImGuiHelper.h>

#include "Renderer.h"

using PBR::TransformationData;
using PBR::MaterialPropertyData;
using PBR::PBRMeshRenderProxy;
using PBR::RendererUniformType;
using PBR::LightingData;
using PBR::PerspectiveCamera;

/* ============================================================================== Camera ============================================================================== */
const float3 PBR::PerspectiveCamera::WorldUp = float3(0, 1, 0);

PerspectiveCamera::PerspectiveCamera(): Position({0, 0, 10}), Front({0, 0, -1}), Up({0, 1, 0}), Right({1, 0, 0}),
                                        FovDegree(60), Aspect(1920.f / 1080.f), NearPlane(0.1f), FarPlane(100.f)
{
    UpdateVPMat();
}

void PerspectiveCamera::UpdateVPMat()
{
    float4x4 viewMat = glm::lookAt(Position, Position + Front, Up);
    float4x4 projMat = glm::perspective(glm::radians(FovDegree), Aspect, NearPlane, FarPlane);
    m_vpMat = projMat * viewMat;
}

void PerspectiveCamera::MoveForward(float dist)
{
    Position += dist * Front;
}

void PerspectiveCamera::MoveUpward(float dist)
{
    Position += dist * Up;
}
/* =================================================================================================================================================================== */


/* ========================================================================== ImGUI Update ========================================================================== */
void PBR::UpdateUI(ImGuiSharedGlobals* ImGlobals)
{

    ImGui::SetCurrentContext(ImGlobals->Context);
    ImGui::SetAllocatorFunctions(ImGlobals->MemAllocFunc, ImGlobals->MemFreeFunc, ImGlobals->UserData);
    static bool show_demo_window = true;
    static bool show_another_window = true;
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

        //viewMat = glm::rotate(glm::mat4(1.f), DeltaX * 0.01f, glm::vec3(0.0f, 1.0f, 0.0f)) * viewMat;
        //viewMat = glm::rotate(glm::mat4(1.f), DeltaY * 0.01f, glm::vec3(1.0f, 0.0f, 0.0f)) * viewMat;
        float3 PlayerMove = { 0.f, 0.f, 0.f };
        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            PlayerMove += float3(0., 0., 1.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            PlayerMove += float3(0., 0., -1.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            PlayerMove += float3(1., 0., 0.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            PlayerMove += float3(-1., 0., 0.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q))
        {
            PlayerMove += float3(0., -1., 0.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_E))
        {
            PlayerMove += float3(0., 1., 0.);
        }
        //viewMat = glm::translate(glm::mat4(1.0), PlayerMove * 0.1f) * viewMat;
    }
}

/* ================================================================================================================================================================ */

void PBRMeshRenderProxy::Initialize(RendererContext* Context, Mesh& InMesh, MaterialPropertyData* InMaterial)
{
    material = InMaterial;

    RHIVertexBuffer.Initialize(&Context->Context, sizeof(Mesh::VertexType), InMesh.Vertices.size(), BufferType::VERTEX);
    RHIIndexBuffer.Initialize(&Context->Context, sizeof(Mesh::VertexType), InMesh.Indices.size(), BufferType::INDEX);

    RHIVertexBuffer.CopyToBuffer(&Context->Context, InMesh.Vertices.data(),
                                 InMesh.Vertices.size() * sizeof(Mesh::VertexType));
    RHIIndexBuffer.CopyToBuffer(&Context->Context, InMesh.Indices.data(), InMesh.Indices.size() * sizeof(uint32_t));

    IndexBufferSize = InMesh.Indices.size();
}

std::unordered_map<PBR::RendererUniformType, uint32_t> PBRRenderer::UniformBindings
{
    {RendererUniformType::Transformation, 0},
    {RendererUniformType::MaterialProperty, 1},
    {RendererUniformType::Lighting, 2}
};

uint32_t PBRRenderer::GetUniformBinding(PBR::RendererUniformType uniformType)
{
    if (UniformBindings.find(uniformType) != UniformBindings.end())
    {
        return UniformBindings[uniformType];
    }
    return -1;
}


void PBRRenderer::AddSceneObject(PBRMeshRenderProxy& MeshProxy)
{
    MaterialMap[MeshProxy.material].push_back(&MeshProxy);
}


void PBRRenderer::Initialize(RendererContext* rendererContext, std::vector<char>& vs, std::vector<char>& ps)
{
    RContext = rendererContext;

    /* Graphics Dispatcher */
    GraphicDispatcher.Initialize(&RContext->Context);

    /* Render Pass */
    RContext->WindowManager.InitializeRenderPassAsPresent(&PresentPass, &RContext->Context);

    /* PSO */
    PipelineFactory.SetShaders(vs, ps);

    PipelineFactory.AddBufferBinding(0, sizeof(Mesh::VertexType));
    PipelineFactory.AddBufferLayout(0, 0, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Position));
    PipelineFactory.AddBufferLayout(0, 1, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Color));
    PipelineFactory.AddBufferLayout(0, 2, R32G32_SFLOAT, offsetof(Mesh::VertexType, TexCoord));
    PipelineFactory.AddBufferLayout(0, 3, R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Normal));

    PipelineFactory.SetUniformBinding(0); // MVP etc.
    PipelineFactory.SetUniformBinding(1); // PBR Material
    PipelineFactory.SetUniformBinding(2); // Lighting

    TransformationRelatedUniform.Initialize(&RContext->Context, sizeof(TransformationData));
    MaterialPropertyRelatedUniform.Initialize(&RContext->Context, sizeof(MaterialPropertyData));
    LightingRelatedUniform.Initialize(&RContext->Context, sizeof(LightingData));

    PipelineFactory.InitializePipelineObject(&PresentPipelineObject, &RContext->Context, &PresentPass);

    /* ImGUI */
    RContext->ImGUI.Initialize(&RContext->Context, &RContext->WindowManager, &PresentPass);
}


bool PBRRenderer::SetUniform(RendererUniformType uniformType, void* data)
{
    bool success = false;
    uint32_t bindingLocation;
    TransformationData* transformData;
    LightingData* lightingData;
    MaterialPropertyData* materialData;
    switch (uniformType)
    {
    case RendererUniformType::Transformation:
        transformData = static_cast<TransformationData*>(data);
        TransformationRelatedUniform.CopyToBuffer(&RContext->Context, transformData, sizeof(*transformData));
        PresentPipelineObject.SetUniform(&TransformationRelatedUniform,
                                         GetUniformBinding(RendererUniformType::Transformation));
        success = true;
        break;
    case RendererUniformType::Lighting:
        lightingData = static_cast<LightingData*>(data);
        LightingRelatedUniform.CopyToBuffer(&RContext->Context, lightingData, sizeof(*lightingData));
        PresentPipelineObject.SetUniform(&LightingRelatedUniform, GetUniformBinding(RendererUniformType::Lighting));
        success = true;
        break;
    case RendererUniformType::MaterialProperty:
        // TODO: Remove the following, material bound with PBRMeshProxy, shouldn't set manually
        materialData = static_cast<MaterialPropertyData*>(data);
        MaterialPropertyRelatedUniform.CopyToBuffer(&RContext->Context, materialData, sizeof(*materialData));
        PresentPipelineObject.SetUniform(&MaterialPropertyRelatedUniform,
                                         GetUniformBinding(RendererUniformType::MaterialProperty));
        break;
    default:
        break;
    }
    return success;
}

void PBRRenderer::BindMaterial(PBR::MaterialPropertyData* materialData)
{
    // TODO: change to descriptor binding instead of coherent mapped memory?
    MaterialPropertyRelatedUniform.CopyToBuffer(&RContext->Context, materialData, sizeof(*materialData));
    uint32_t bindingLocation = GetUniformBinding(RendererUniformType::MaterialProperty);
    PresentPipelineObject.SetUniform(&MaterialPropertyRelatedUniform, bindingLocation);
}

void PBRRenderer::UpdateFrame()
{
    auto& rhiContext = RContext->Context;
    auto& windowManager = RContext->WindowManager;

    GraphicDispatcher.WaitForGPUIdle(&rhiContext);
    GraphicDispatcher.BeginFrame(&rhiContext, &windowManager, &PresentPass);
    {
        RContext->ImGUI.UpdateUI(mp_funcImDraw);

        // PBR Draw
        GraphicDispatcher.BeginRenderPass(&PresentPass);
        {
            for (const auto& [material, meshProxyList] : MaterialMap)
            {
                BindMaterial(material);
                // TODO: replace the following with draw instanced (batch?)
                for (PBRMeshRenderProxy* meshProxy : meshProxyList)
                {
                    GraphicDispatcher.BindIndexBuffer(&meshProxy->RHIIndexBuffer, 0);
                    GraphicDispatcher.BindVertexBuffer(&meshProxy->RHIVertexBuffer, 0, 0);
                    GraphicDispatcher.Dispatch(&PresentPipelineObject, meshProxy->IndexBufferSize, 0, 1);
                }
                // break;  // TODO: remove the break for multiple materials (currently bugged)
            }
            RContext->ImGUI.DispatchImGUI(&GraphicDispatcher);
        }
        GraphicDispatcher.EndRenderPass(&PresentPass);
    }
    GraphicDispatcher.EndFrameAndSubmit(&rhiContext, &windowManager);
}
