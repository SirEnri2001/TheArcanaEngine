#define RHI_INCLUDE
#include <chrono>

#include "RHI.h"

#include "CoreMath.h"
#include "CoreMath.inl"

#define TINYOBJLOADER_IMPLEMENTATION
#include <unordered_map>

#include "tiny_obj_loader.h"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";
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
};

struct Vertex {
    float3 pos;
    float3 color;
    float2 texCoord;

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<float3>()(vertex.pos) ^ (hash<float3>()(vertex.color) << 1)) >> 1)
                ^ (hash<float2>()(vertex.texCoord) << 1);
        }
    };
}


void loadModel(std::vector<Vertex>& OutVertices, std::vector<uint32_t>& OutIndices, const std::string infile) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, infile.c_str(), nullptr, false)) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = { 1.0f, 1.0f, 1.0f };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(OutVertices.size());
                OutVertices.push_back(vertex);
            }

            OutIndices.push_back(uniqueVertices[vertex]);
        }
    }
}

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

    OutUniformBufferObject.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    OutUniformBufferObject.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    OutUniformBufferObject.proj = glm::perspective(glm::radians(45.0f), float(WIDTH) / float(HEIGHT), 0.1f, 10.0f);
    OutUniformBufferObject.proj[1][1] *= -1;
}

int main()
{
	// Init RHI Context & window
	// create pipeline
	// create buffers
	// set rendering parameters
	// executing shaders
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

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
    //loadModel(vertices, indices, MODEL_PATH);
    vertices.push_back(Vertex{ float3(-1., 0.,0.), float3(1.,0.,0.), float2(0.,0.) });
    vertices.push_back(Vertex{ float3( 1., 0.,0.), float3(0.,1.,0.), float2(1.,0.) });
    vertices.push_back(Vertex{ float3( 0., 1.,0.), float3(1.,0.,1.), float2(0.,1.) });
    indices.push_back(0);
    indices.push_back(2);
    indices.push_back(1);

    Context.Initialize();
    Uniform.Initialize(&Context, sizeof(UniformBufferObject));
    RHIVertexBuffer.Initialize(&Context, sizeof(Vertex), vertices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    RHIIndexBuffer.Initialize(&Context, sizeof(Vertex), vertices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    RHIVertexBuffer.CopyToBuffer(&Context, vertices.data(), vertices.size() * sizeof(Vertex));
    RHIIndexBuffer.CopyToBuffer(&Context, indices.data(), indices.size() * sizeof(uint32_t));
    Texture.Initialize(&Context, TEXTURE_PATH.c_str());

    Pipeline.AddBinding(0, sizeof(Vertex));
    Pipeline.AddLayout(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos));
    Pipeline.AddLayout(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color));
    Pipeline.AddLayout(0, 2, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, texCoord));
    //Pipeline.AddUniformBuffer({ Uniform.Buffer, 0, sizeof(UniformBufferObject) });
    //Pipeline.AddImageSampler({ Texture.Sampler, Texture.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
    Pipeline.Initialize(&Context, VertexShaderSPIRV, FragmentShaderSPIRV);

    GraphicDispatcher.BindIndexBuffer(&RHIIndexBuffer, 0);
    GraphicDispatcher.BindVertexBuffer(&RHIVertexBuffer, 0, 0);
    GraphicDispatcher.Initialize(&Context);

    GraphicDispatcher.Start(&Context, &Pipeline, indices.size(), 0, 1);

	return 0;
}