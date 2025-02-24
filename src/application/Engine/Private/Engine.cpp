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
    while (Context.WindowActive())
    {
        updateUniformBuffer(ubo);
        Uniform.CopyToBuffer(&Context, &ubo, sizeof(ubo));
        GraphicDispatcher.Dispatch(&Context, &Pipeline, StaticMesh.Indices.size(), 0, 1);
    }

	return 0;
}