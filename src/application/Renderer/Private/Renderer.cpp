#define RENDERER_IMPLEMENT
#include "Renderer.h"

#include <chrono>
#include <CoreLog.inl>

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

void MeshRenderProxy::Initialize(RendererContext* Context, Mesh& InMesh)
{
	RHIVertexBuffer.Initialize(&Context->Context, sizeof(Mesh::VertexType), InMesh.Vertices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	RHIIndexBuffer.Initialize(&Context->Context, sizeof(Mesh::VertexType), InMesh.Vertices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	RHIVertexBuffer.CopyToBuffer(&Context->Context, InMesh.Vertices.data(), InMesh.Vertices.size() * sizeof(Mesh::VertexType));
	RHIIndexBuffer.CopyToBuffer(&Context->Context, InMesh.Indices.data(), InMesh.Indices.size() * sizeof(uint32_t));
	Texture.Initialize(&Context->Context, InMesh.TexturePath.c_str());
    IndexBufferSize = InMesh.Indices.size();
}

MeshRenderProxy::~MeshRenderProxy()
{
    Log("Release Render Proxy");
}


void Renderer::Initialize(RendererContext* Context, std::vector<char> VS, std::vector<char> PS)
{
    Pipeline.SetShaders(VS, PS);
    Pipeline.AddBinding(0, sizeof(Mesh::VertexType));
    Pipeline.AddLayout(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Position));
    Pipeline.AddLayout(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Color));
    Pipeline.AddLayout(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(Mesh::VertexType, TexCoord));
    Pipeline.AddLayout(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Mesh::VertexType, Normal));
    Pipeline.Initialize(&Context->Context, &Context->RenderPass);
    GraphicDispatcher.Initialize(&Context->Context);
}

void Renderer::AddUniform(RHIVulkanUniform Uniform, uint32_t Binding)
{
    Pipeline.AddUniformBuffer({ Uniform.Buffer, 0, Uniform.Size}, Binding);
}

void Renderer::AddTextureSampler(RHIVulkanImageResource Texture, uint32_t Binding)
{
    Pipeline.AddImageSampler({ Texture.Sampler, Texture.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, Binding);
}


void Renderer::DrawScene(RendererContext* Context, MeshRenderProxy& InMeshProxy)
{
    MeshProxyPasses.push_back(&InMeshProxy);
}

void Renderer::UpdateUI()
{
    static bool show_demo_window = true;
    static bool show_another_window = true;
    static float4 clear_color;
    static ImGuiIO& io = ImGui::GetIO();
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    if (show_demo_window)
    {
        ImGui::ShowDemoWindow(&show_demo_window);
    }

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
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
        //viewMat = glm::translate(glm::mat4(1.0), PlayerMove * 0.001f) * viewMat;
    }
    // Rendering
    ImGui::Render();
}

void Renderer::UpdateFrame(RendererContext* RContext)
{
    auto& Context = RContext->Context;
    UpdateUI();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    uint32_t ImageIndex;
    GraphicDispatcher.PrepareRenderPass(&Context, ImageIndex, &RContext->RenderPass);
    GraphicDispatcher.BeginRenderPass(&Context, &RContext->RenderPass, ImageIndex);

    for (auto& MeshProxy : MeshProxyPasses)
    {
        GraphicDispatcher.BindIndexBuffer(&MeshProxy->RHIIndexBuffer, 0);
        GraphicDispatcher.BindVertexBuffer(&MeshProxy->RHIVertexBuffer, 0, 0);
        IndexBufferSize = MeshProxy->IndexBufferSize;
        GraphicDispatcher.Dispatch(&Context, &Pipeline, IndexBufferSize, 0, 1);
    }
    // Comment this line if you don't want ImGUI
    GraphicDispatcher.DispatchImGUI(draw_data, ImGui_ImplVulkan_RenderDrawData);

    GraphicDispatcher.EndRenderPass();
    GraphicDispatcher.Submit(&Context, ImageIndex);
}


RendererContext* RendererContext::Get()
{
	if (GInstance==nullptr)
	{
		GInstance = new RendererContext();
	}
	return GInstance;
}

void RendererContext::Initialize(int Width, int Height)
{
	if (bInitialized)
	{
		return;
	}
	Context.Initialize({ Width, Height });
	RenderPass.Initialize(&Context);
	ImGui::CreateContext();
	ImGui_ImplVulkan_InitInfo ImGuiInitInfo{};
	ImGuiInitInfo.Instance = Context.Instance;
	ImGuiInitInfo.PhysicalDevice = Context.PhysicalDevice;
	ImGuiInitInfo.Device = Context.Device;
	ImGuiInitInfo.QueueFamily = Context.GraphicsQueueFamilyIndex;
	ImGuiInitInfo.Queue = Context.GraphicsQueue;
	ImGuiInitInfo.DescriptorPool = nullptr;
	ImGuiInitInfo.DescriptorPoolSize = 32;
	ImGuiInitInfo.RenderPass = RenderPass.RenderPass;
	ImGuiInitInfo.MinImageCount = 2;
	ImGuiInitInfo.ImageCount = Context.SwapchainImageViews.size();
	ImGuiInitInfo.MSAASamples = RenderPass.msaaSamples;

	ImGui_ImplVulkan_Init(&ImGuiInitInfo);
	ImGui_ImplGlfw_InitForVulkan(Context.pGLFWwindow, true);
}

bool RendererContext::IsWindowAlive()
{
    return Context.WindowActive();
}


RendererContext* RendererContext::GInstance = nullptr;