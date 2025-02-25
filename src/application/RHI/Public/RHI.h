#pragma once

#ifdef RHI_IMPLEMENT
#define RHI_API __declspec(dllexport)
#else
#ifdef RHI_INCLUDE
#define RHI_API __declspec(dllimport)
#else
#error Please specify API linkage before include this file
#define RHI_API
#endif
#endif
#include <cstdint>
#include <vulkan/vulkan_core.h>

#include "RHIVulkan.h"

#include <vector>
#include <fstream>

struct ImDrawData;

class RHI_API RHIVulkanContext
{
public:
    bool bIsValid = false;
    struct VulkanContextInfo
    {
        int WindowWidth;
        int WindowHeight;
    } Info;

    VkInstance Instance;
    VkDebugUtilsMessengerEXT DebugMessenger;
    VkSurfaceKHR Surface;
    VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
    VkDevice Device = VK_NULL_HANDLE;

    std::vector<VkPhysicalDevice> AvailablePhysicalDevices;
    std::vector<const char*> Extensions;
    std::vector<const char*> PhysicalDeviceExtensions;

    VkSwapchainKHR Swapchain;
    std::vector<VkImage> SwapchainImages;
    std::vector<VkImageView> SwapchainImageViews;
    VkFormat SwapchainImageFormat;
    VkExtent2D SwapchainExtent;
    uint32_t GraphicsQueueFamilyIndex;
    uint32_t PresentQueueFamilyIndex;
    VkPhysicalDeviceFeatures supportedFeatures;
    VkSurfaceCapabilitiesKHR SurfaceCapabilities;
    std::vector<VkSurfaceFormatKHR> SurfaceFormats;
    std::vector<VkPresentModeKHR> PresentModes;

    VkQueue GraphicsQueue;
    VkQueue PresentQueue;
    VkCommandPool CommandPool;
    VkPhysicalDeviceMemoryProperties PDMemoryProperties;
    VkFormat DepthFormat;

    GLFWwindow* pGLFWwindow;

    RHIVulkanContext(VulkanContextInfo CreateInfo);

    void Initialize();

    void CleanupSwapchain();

    void Cleanup();

    void InitializeSwapchain();

    static void OnWindowResize(GLFWwindow* window, int width, int height);

    void WaitDeviceIdle();

    bool WindowActive();
};

class RHI_API RHIVulkanImageResource
{
public:
    VkImage Image;
    VkDeviceMemory DeviceMemory;
    VkImageView ImageView;
    VkSampler Sampler;
    bool bHasSampler = false;
    void Initialize(RHIVulkanContext* Context, VkExtent3D ImageExtent, uint32_t MipLevel, VkSampleCountFlagBits numSamples,
        VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlagBits ImageAspectFlagBits, VkMemoryPropertyFlags properties);

    void Initialize(RHIVulkanContext* Context, const char* ImageFileName, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT,
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
        VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VkImageAspectFlagBits ImageAspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT, VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    void Cleanup(RHIVulkanContext* Context);
};

class RHI_API RHIVulkanBufferResource
{
public:
    VkBuffer Buffer;
    VkDeviceMemory DeviceMemory;

    void Initialize(RHIVulkanContext* Context, uint32_t Stride, uint32_t ElementCounts, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties);

    void CopyToBuffer(RHIVulkanContext* Context, void* data, uint32_t TotalBytes);

    void Cleanup(RHIVulkanContext* Context);
};

class RHI_API RHIVulkanUniform
{
public:
    VkBuffer Buffer;
    VkDeviceMemory DeviceMemory;
    void* MappedMemory;
    VkDescriptorBufferInfo DescriptorBufferInfo;
    void Initialize(RHIVulkanContext* Context, uint32_t UniformStructSize);

    void CopyToBuffer(RHIVulkanContext* Context, void* data, uint32_t TotalBytes);

    void Cleanup(RHIVulkanContext* Context);
};

class RHI_API RHIVulkanPipeline
{
    std::vector<char> VertShaderBytecode;
    std::vector<char> FragShaderBytecode;
public:
    VkRenderPass RenderPass;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    VkDescriptorPool DescriptorPool;
    VkDescriptorSetLayout DescriptorSetLayout;
    VkPipeline Pipeline;
    VkPipelineLayout PipelineLayout;
    std::vector<VkFramebuffer> SwapchainFramebuffers;

    std::vector<VkVertexInputBindingDescription> BindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> AttributeDescriptions;

    VkDescriptorSet DescriptorSet;
    std::vector<VkWriteDescriptorSet> WriteDescriptorSets;
    uint32_t UniformBufferDescriptorCount = 0;
    uint32_t CombinedImageSamplerDescriptorCount = 0;


    RHIVulkanImageResource* ColorRenderTargetResource;
    RHIVulkanImageResource* DepthRenderTargetResource;

    void AddLayout(uint32_t BindingIndex, uint32_t Location, VkFormat Format, uint32_t Offset);

    void AddBinding(uint32_t BindingIndex, uint32_t Stride);

    void AddUniformBuffer(const VkDescriptorBufferInfo& UniformDescBufferInfo);

    void AddImageSampler(const VkDescriptorImageInfo& DescImageInfo);

    void CreateSwapchainFramebuffer(RHIVulkanContext* Context, VkImageView ColorRenderTargetImageView, VkImageView DepthRenderTargetImageView, VkRenderPass RenderPass);

    void Initialize(RHIVulkanContext* Context, const std::vector<char>& VertShader, const std::vector<char>& FragShader);

    void CleanupSwapchainFramebuffer(RHIVulkanContext* Context);

    void Cleanup(RHIVulkanContext* Context);
};

class RHI_API RHIVulkanImGUI
{
public:
    void Initialize(RHIVulkanContext* Context);
    void Dispatch(RHIVulkanContext* Context);
};

class RHI_API RHIVulkanGraphicDispatcher
{
public:
    VkSemaphore ImageAvailableSemaphore;
    VkSemaphore RenderFinishSemaphore;
    VkFence InFlightFence;
    VkCommandBuffer CommandBuffer;

    struct BindingInfo
    {
        RHIVulkanBufferResource* BufferResource;
        VkDeviceSize Offset;
        uint32_t BindingIndex;
    };

    std::vector<BindingInfo> BindingInfos;
    BindingInfo IndexBindingInfo;

    void Initialize(RHIVulkanContext* Context);

    void Cleanup(RHIVulkanContext* Context);

    void BindVertexBuffer(RHIVulkanBufferResource* BufferResource, VkDeviceSize Offset, uint32_t BindingIndex);

    void BindIndexBuffer(RHIVulkanBufferResource* BufferResource, VkDeviceSize Offset);

    void Dispatch(RHIVulkanContext* Context, RHIVulkanPipeline* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount);

	void DispatchImGUI(RHIVulkanContext* Context, RHIVulkanPipeline* Pipeline, ImDrawData* draw_data,
        void (*ImGui_ImplVulkan_RenderDrawData)(ImDrawData* draw_data, VkCommandBuffer command_buffer, VkPipeline pipeline));

    void PrepareRenderPass(RHIVulkanContext* Context, uint32_t& OutImageIndex);

	void BeginRenderPass(RHIVulkanContext* Context, RHIVulkanPipeline* Pipeline, uint32_t ImageIndex);

	void EndRenderPass();

	void Submit(RHIVulkanContext* Context, uint32_t ImageIndex);
};

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);