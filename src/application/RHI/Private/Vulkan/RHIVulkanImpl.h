// RHIVulkanImpl.h - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
// @description Vulkan helper functions

#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>


class VulkanAPILoader
{
public:
    VkInstance Instance;

    // Vulkan api loaders
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pMessenger)
    {
        static auto myvkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT"));
        return myvkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
    }

    VKAPI_ATTR void VKAPI_CALL vkCmdPushDescriptorSetKHR(
        VkCommandBuffer commandBuffer,
        VkPipelineBindPoint pipelineBindPoint,
        VkPipelineLayout layout,
        uint32_t set,
        uint32_t descriptorWriteCount,
        const VkWriteDescriptorSet* pDescriptorWrites)
    {
        static auto myvkCmdPushDescriptorSetKHR = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(vkGetInstanceProcAddr(
            Instance, "vkCmdPushDescriptorSetKHR"));
        return myvkCmdPushDescriptorSetKHR(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount,
            pDescriptorWrites);
    }
};

extern VulkanAPILoader APILoader;

struct GLFWwindow;
struct GLFWmonitor;

void CreateGLFWContext();
void CreateGLFWWindow(GLFWwindow*& pGLFWwindow, int width, int height, void* CallbackOwner, void (*framebufferResizeCallback)(GLFWwindow* window, int width, int height));

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

void CreateCommandBuffer(VkCommandBuffer& OutCommandBuffer, VkDevice Device, VkCommandPool CommandPool);

void BeginCommandBufferOneTimeSubmit(VkCommandBuffer& InCommandBuffer);

void EndCommandBufferOneTimeSubmit(VkCommandBuffer InCommandBuffer, VkQueue graphicsQueue);

void CreateVkInstance(
    VkInstance& OutVkInstance,
    VkDebugUtilsMessengerCreateInfoEXT& OutDebugMsgCreateInfo,
    VkDebugUtilsMessengerEXT& OutDebugMessenger,
    std::vector<const char*>& InOutExtensions,
    PFN_vkDebugUtilsMessengerCallbackEXT DebugCallback = debugCallback
);

void CreateVkSurface(VkInstance& Instance, GLFWwindow* Window, VkSurfaceKHR& OutVkSurface);

void CreateVkDevice(VkDevice& OutVkDevice, VkQueue& OutGraphicsQueue, VkQueue& OutPresentQueue,
    VkPhysicalDevice physicalDevice, uint32_t GraphicsFamilyIndex, uint32_t PresentFamilyIndex, const std::vector<const char*>& Extensions);

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

void chooseSwapExtent(VkExtent2D& InOutExtent, const VkSurfaceCapabilitiesKHR& capabilities);

void CreateImageView(VkImageView& OutImageView, VkImage image, VkDevice Device, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

void CreateSwapchain(
    VkSwapchainKHR& OutSwapchain,
    std::vector<VkImage>& OutSwapchainImages,
    std::vector<VkImageView>& OutSwapchainImageViews,
    VkFormat& OutSwapchainImageFormat,
    VkExtent2D& InOutSwapchainExtent,
    const VkDevice& Device,
    const VkSurfaceKHR& Surface,
    const VkSurfaceCapabilitiesKHR& SurfaceCapabilities,
    const std::vector<VkSurfaceFormatKHR>& SurfaceFormats,
    const std::vector<VkPresentModeKHR>& PresentModes,
    uint32_t GraphicsFamilyIndex, uint32_t PresentFamilyIndex
);

VkSampleCountFlagBits getMaxUsableSampleCount(const VkPhysicalDevice& physicalDevice);

void RetrieveAvailablePhysicalDevices(std::vector<VkPhysicalDevice>& OutAvailablePhysicalDevices, const VkInstance& instance, const std::vector<const char*>& extensions);

bool PhysicalDevideSupportQueueFamilies(uint32_t& OutGraphicsFamily, uint32_t& OutPresentFamily, VkPhysicalDevice PhysicalDevice, VkSurfaceKHR surface);

bool PhysicalDeviceSupportExtensions(VkPhysicalDevice device, std::vector<const char*> extensions);

bool PhysicalDeviceSupportSurface(
    VkSurfaceCapabilitiesKHR& OutSurfaceCapabilities,
    std::vector<VkSurfaceFormatKHR>& OutSurfaceFormats,
    std::vector<VkPresentModeKHR>& OutPresentModes,
    VkPhysicalDevice PhysicalDevice, VkSurfaceKHR surface
);

VkFormat findSupportedFormat(VkPhysicalDevice PhysicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

VkFormat findDepthFormat(VkPhysicalDevice PhysicalDevice);

void CreateRenderPassSingleSubpass(VkRenderPass& OutRenderPass, VkDevice Device, const std::vector<VkAttachmentDescription>& Attachments, int32_t DepthAttachementIndex);
void CreatePresentableRenderPass(VkRenderPass& OutRenderPass, VkDevice Device, bool hasDepth, VkFormat DepthFormat, VkFormat SwapchainImageFormat, VkSampleCountFlagBits msaaSamples);

void CreateDescriptorSetLayout(VkDescriptorSetLayout& OutDescriptorSetLayout, const std::vector<VkDescriptorSetLayoutBinding>& DescSetLayoutBindings, VkDevice Device);

void CreateGraphicsPipeline(VkPipeline& OutGraphicsPipeline, VkPipelineLayout& OutPipelineLayout, VkSampleCountFlagBits SampleCountFlagBits,
    VkDevice device, const std::vector<char>& VertShaderBytecode, const char* VertShaderMain, const std::vector<char>& FragShaderBytecode, const char* FragShaderMain,
    const std::vector<VkVertexInputBindingDescription>& BindingDescriptions, const std::vector<VkVertexInputAttributeDescription>& AttributeDescriptions,
    const VkDescriptorSetLayout& DescriptorSetLayout, const VkRenderPass& RenderPass);

void CreateComputePipeline(VkPipeline& OutComputePipeline, VkPipelineLayout& OutPipelineLayout,
    VkDevice device, const std::vector<char>& ComputeShaderBytecode, const char* ComputeShaderMain,
    const VkDescriptorSetLayout& DescriptorSetLayout);

void CreateCommandPool(VkCommandPool& OutCommandPool, VkDevice Device, uint32_t GraphcisFamilyIndex);

void GetMemoryRequirement(VkMemoryRequirements& OutMemoryRequirement, VkDevice Device, VkImage Image);

void GetMemoryRequirement(VkMemoryRequirements& OutMemoryRequirement, VkDevice Device, VkBuffer Buffer);

void CreateDeviceMemory(VkDeviceMemory& OutDeviceMemory, VkDevice Device, uint32_t Size, uint32_t MemoryTypeIndex);

void CreateImage(VkImage& Image, 
    VkDevice Device, VkExtent3D Extent, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage);

void CreateBuffer(VkBuffer& OutBuffer, VkDevice Device, VkDeviceSize size, VkBufferUsageFlags usage);

//void TransitionImageLayout(VkImage image, VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

void TransitionImageLayout(
    VkImage image,
    VkCommandBuffer commandBuffer,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags srcPipelineStageMask,
    VkPipelineStageFlags dstPipelineStageMask,
    uint32_t mipLevels);

void CopyBufferToImage(VkBuffer buffer, VkImage image, VkCommandBuffer commandBuffer, uint32_t width, uint32_t height);

void CreateSampler(VkSampler& OutSampler, VkDevice Device, float maxSamplerAnisotropy);

void CopyBuffer(VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize Size, VkCommandBuffer CommandBuffer);

void CreateDescriptorPool(VkDescriptorPool& OutDescriptorPool, VkDevice Device, uint32_t UniformBufferCount = 16, uint32_t CombinedImageSamplerCount = 16);

void CreateDescriptorSet(VkDescriptorSet& OutDescriptorSet, std::vector<VkWriteDescriptorSet>& InOutWriteDescriptorSets,
    VkDevice Device, VkDescriptorPool DescriptorPool, const VkDescriptorSetLayout& DescriptorSetLayout);

VkSampleCountFlagBits GetMaxUsableSampleCount(VkPhysicalDevice& PhysicalDevice);

// Create mipmap level for image & set image layout to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
void CreateMipmapForImage(VkCommandBuffer& InCommandBuffer, VkImage Image, int32_t TexWidth, int32_t TexHeight, uint32_t MipmapLevel);

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
