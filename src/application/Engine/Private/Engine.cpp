#define RHI_INCLUDE
#define COREGEOMETRY_INCLUDE
#include <chrono>

#include "RHI.h"

#include "CoreMath.h"
#include "CoreMath.inl"
#include "CoreGeometry.h"

#include <unordered_map>

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"


#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::string MODEL_PATH = "models/spot/spot_triangulated_good.obj";
const std::string TEXTURE_PATH = "models/spot/spot_texture.png";
const std::string VERT_SHADER_PATH = "shaders/vert.spv";
const std::string FRAG_SHADER_PATH = "shaders/frag.spv";

//class Engine
//{
//public:
//	void InitializeRHIContext();
//	void CreatePipeline();
//	void CreateBuffers();
//	void SetShaderParameters();
//	void Draw();
//	void Tick();
//};


struct UniformBufferObject {
    alignas(16) float4x4 model;
    alignas(16) float4x4 view;
    alignas(16) float4x4 proj;
    alignas(16) float4 viewPosition;
};

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

void updateUniformBuffer(UniformBufferObject& OutUniformBufferObject) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    OutUniformBufferObject.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    OutUniformBufferObject.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    OutUniformBufferObject.proj = glm::perspective(glm::radians(45.0f), float(WIDTH) / float(HEIGHT), 0.1f, 10.0f);
    OutUniformBufferObject.proj[1][1] *= -1;
    OutUniformBufferObject.viewPosition = float4(2.f, 2.f, 2.f, 1.f);
}

int main()
{
    typedef Vertex<float3, float3, float2, float3, float3> VertexType;
    typedef TMesh<VertexType, uint32_t> MeshType;

    MeshType StaticMesh = MeshType::LoadObj(MODEL_PATH);
    UniformBufferObject ubo;
    // RHI Objects
    RHIVulkanContext Context({WIDTH, HEIGHT});
    RHIVulkanPipeline Pipeline;
    RHIVulkanUniform Uniform;
    RHIVulkanBufferResource RHIVertexBuffer;
    RHIVulkanBufferResource RHIIndexBuffer;
    RHIVulkanImageResource Texture;
    RHIVulkanGraphicDispatcher GraphicDispatcher;

    std::vector<char> VertexShaderSPIRV = readFile(VERT_SHADER_PATH);
    std::vector<char> FragmentShaderSPIRV = readFile(FRAG_SHADER_PATH);
    ImGui::CreateContext();
    ImGui_ImplVulkan_InitInfo ImGuiInitInfo{};
    Context.Initialize();
    Uniform.Initialize(&Context, sizeof(UniformBufferObject));
    RHIVertexBuffer.Initialize(&Context, sizeof(VertexType), StaticMesh.Vertices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    RHIIndexBuffer.Initialize(&Context, sizeof(VertexType), StaticMesh.Vertices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    RHIVertexBuffer.CopyToBuffer(&Context, StaticMesh.Vertices.data(), StaticMesh.Vertices.size() * sizeof(VertexType));
    RHIIndexBuffer.CopyToBuffer(&Context, StaticMesh.Indices.data(), StaticMesh.Indices.size() * sizeof(uint32_t));
    Texture.Initialize(&Context, TEXTURE_PATH.c_str());

    Pipeline.AddBinding(0, sizeof(VertexType));
    Pipeline.AddLayout(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexType, Position));
    Pipeline.AddLayout(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexType, Color));
    Pipeline.AddLayout(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexType, TexCoord));
    Pipeline.AddLayout(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexType, Normal));
    Pipeline.AddUniformBuffer({ Uniform.Buffer, 0, sizeof(UniformBufferObject) });
    Pipeline.AddImageSampler({ Texture.Sampler, Texture.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    Pipeline.Initialize(&Context, VertexShaderSPIRV, FragmentShaderSPIRV);

    GraphicDispatcher.BindIndexBuffer(&RHIIndexBuffer, 0);
    GraphicDispatcher.BindVertexBuffer(&RHIVertexBuffer, 0, 0);
    GraphicDispatcher.Initialize(&Context);

    ImGuiInitInfo.Instance = Context.Instance;
    ImGuiInitInfo.PhysicalDevice = Context.PhysicalDevice;
    ImGuiInitInfo.Device = Context.Device;
    ImGuiInitInfo.QueueFamily = Context.GraphicsQueueFamilyIndex;
    ImGuiInitInfo.Queue = Context.GraphicsQueue;
    ImGuiInitInfo.DescriptorPool = Pipeline.DescriptorPool;
    ImGuiInitInfo.DescriptorPoolSize = 0;
    ImGuiInitInfo.RenderPass = Pipeline.RenderPass;
    ImGuiInitInfo.MinImageCount = 2;
    ImGuiInitInfo.ImageCount = Context.SwapchainImageViews.size();
    ImGuiInitInfo.MSAASamples = Pipeline.msaaSamples;

    ImGui_ImplVulkan_Init(&ImGuiInitInfo);
    ImGui_ImplGlfw_InitForVulkan(Context.pGLFWwindow, true);
    bool show_demo_window = true;
    bool show_another_window = true;
    float4 clear_color;
    while (Context.WindowActive())
    {
        updateUniformBuffer(ubo);
        Uniform.CopyToBuffer(&Context, &ubo, sizeof(ubo));
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
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        uint32_t ImageIndex;
        GraphicDispatcher.PrepareRenderPass(&Context, ImageIndex);
        GraphicDispatcher.BeginRenderPass(&Context, &Pipeline, ImageIndex);
        GraphicDispatcher.Dispatch(&Context, &Pipeline, StaticMesh.Indices.size(), 0, 1);

        // Comment this line if you don't want ImGUI
        GraphicDispatcher.DispatchImGUI(&Context, &Pipeline, draw_data, ImGui_ImplVulkan_RenderDrawData);

        GraphicDispatcher.EndRenderPass();
        GraphicDispatcher.Submit(&Context, ImageIndex);

    }
	return 0;
}
