// RHIVulkan.h - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
// @description RHI Vulkan implementation

#pragma once
#include <stdexcept>
#include <vector>

#include "vulkan/vulkan_core.h"

#include "RHI.h"
#include "RHIImGuiHelper.h"

// Forward declarations
class RHIVulkanWindowManager;
struct GLFWwindow;
struct ImDrawData;

class RHIVulkanPlatformSupport : public RHIPlatformSupportBase
{
    static RHIVulkanPlatformSupport* GInstance;
public:
    VkInstance Instance;
    std::vector<const char*> InstanceExtensions;
    std::vector<const char*> PhysicalDeviceExtensions;
    std::vector<VkPhysicalDevice> AvailablePhysicalDevices;
    VkDebugUtilsMessengerEXT DebugMessenger;

    struct PhysicalDeviceDesc
    {
	    VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
	    VkPhysicalDeviceFeatures PDFeatures;
	    uint32_t GraphicsQueueFamilyIndex;
	    uint32_t PresentQueueFamilyIndex;
        VkPhysicalDeviceProperties PDProperties;
		VkPhysicalDeviceMemoryProperties PDMemoryProperties;
    } CurrentPhysicalDevice;

    RHIVulkanPlatformSupport();

    virtual void Initialize() override;
    virtual void Cleanup() override;
    virtual void InitializePhysicalDevice(RHIWindowManager* WindowManager) override;


    // Vulkan api loaders
	inline VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
	    VkInstance instance,
	    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	    const VkAllocationCallbacks* pAllocator,
	    VkDebugUtilsMessengerEXT* pMessenger) {
	    static PFN_vkCreateDebugUtilsMessengerEXT myvkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT"));
	    return myvkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
	}

	inline VKAPI_ATTR void VKAPI_CALL vkCmdPushDescriptorSetKHR(
	    VkCommandBuffer                             commandBuffer,
	    VkPipelineBindPoint                         pipelineBindPoint,
	    VkPipelineLayout                            layout,
	    uint32_t                                    set,
	    uint32_t                                    descriptorWriteCount,
	    const VkWriteDescriptorSet* pDescriptorWrites) {
	    static PFN_vkCmdPushDescriptorSetKHR myvkCmdPushDescriptorSetKHR = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(vkGetInstanceProcAddr(Instance, "vkCmdPushDescriptorSetKHR"));
	    return myvkCmdPushDescriptorSetKHR(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites);
	}

    // Helpers
    inline VkFormatProperties GetFormatProperties(VkFormat Format)
	{
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(CurrentPhysicalDevice.PhysicalDevice, Format, &props);
        return props;
	}
	    
	VkFormat GetSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	    for (VkFormat format : candidates) {
            auto props = GetFormatProperties(format);
	        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
	            return format;
	        }
	        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
	            return format;
	        }
	    }

	    throw std::runtime_error("failed to find supported format!");
	}

	VkFormat GetDepthFormat() {
	    return GetSupportedFormat(
	        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
	        VK_IMAGE_TILING_OPTIMAL,
	        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	    );
	}

    
	VkSampleCountFlagBits GetMaxUsableSampleCount() {
	    VkSampleCountFlags counts = CurrentPhysicalDevice.PDProperties.limits.framebufferColorSampleCounts & CurrentPhysicalDevice.PDProperties.limits.framebufferDepthSampleCounts;
	    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
	    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
	    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
	    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
	    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
	    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

	    return VK_SAMPLE_COUNT_1_BIT;
	}

    uint32_t GetMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags Properties)
	{
		for (uint32_t i = 0; i < CurrentPhysicalDevice.PDMemoryProperties.memoryTypeCount; i++) {
	        if ((TypeFilter & (1 << i)) && (CurrentPhysicalDevice.PDMemoryProperties.memoryTypes[i].propertyFlags & Properties) == Properties) {
	            return i;
	        }
	    }
	    throw std::runtime_error("failed to find suitable memory type!");
	}

    uint32_t GetMemoryType(VkMemoryRequirements MemoryRequirements, VkMemoryPropertyFlags Properties)
	{
        return GetMemoryType(MemoryRequirements.memoryTypeBits, Properties);
	}

    static VkFormat GetVkFormat(RHIFormat InFormat)
    {
        switch(InFormat)
        {
        case R8G8B8A8_SRGB:
			return VK_FORMAT_R8G8B8A8_SRGB;
        case R32G32B32_SFLOAT:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case R32G32_SFLOAT:
            return VK_FORMAT_R32G32_SFLOAT;
        }
        return VK_FORMAT_UNDEFINED;
    }

    static RHIVulkanPlatformSupport* Get();
};

class RHIVulkanContext : public RHIContextBase
{
public:
    VkDevice Device = VK_NULL_HANDLE;
    VkQueue GraphicsQueue;
    VkQueue PresentQueue;
    VkCommandPool CommandPool;

    RHIVulkanContext();
    virtual ~RHIVulkanContext() override;

    virtual void Initialize(RHIPlatformSupport* PlatformSupport) override;
    virtual void Cleanup() override;
    virtual void WaitDeviceIdle() override;
};

class RHIVulkanResourceFactory
{
public:
    void Initialize();

};

class RHIVulkanWindowManager : public RHIWindowManagerBase
{
public:
    RHIPlatformSupport* PlatformSupport;
    GLFWwindow* pGLFWwindow;
    VkSurfaceKHR Surface;
    VkSurfaceCapabilitiesKHR SurfaceCapabilities;
    std::vector<VkSurfaceFormatKHR> SurfaceFormats;
    std::vector<VkPresentModeKHR> PresentModes;

    struct SwapchainDesc
    {
	    VkSwapchainKHR Swapchain;
	    std::vector<VkImage> SwapchainImages;
		std::vector<VkImageView> SwapchainImageViews;
	    VkExtent2D SwapchainExtent;
	    VkFormat SwapchainImageFormat;
    } CurrentSwapchain;

    RHIVulkanWindowManager() = default;
    virtual ~RHIVulkanWindowManager() override = default;

    virtual void Initialize(RHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth) override;
    virtual void Cleanup(RHIPlatformSupport* PlatformSupport) override;

    virtual void InitializeSwapchain(RHIContext* Context, RHIPlatformSupport* PlatformSupport) override;
    virtual void CleanupSwapchain(RHIContext* Context) override;
    virtual void RecreateSwapchain(RHIContext* Context) override;
    virtual bool IsAlive() override;

    virtual uint32_t GetWindowHeight() override
    {
	    return CurrentSwapchain.SwapchainExtent.height;
    }
    virtual uint32_t GetWindowWidth() override
    {
	    return CurrentSwapchain.SwapchainExtent.width;
    }

    static void OnWindowResize(GLFWwindow* window, int width, int height);
};

class RHIVulkanImageResource : public RHIImageResourceBase
{
public:
    VkImage Image;
    VkDeviceMemory DeviceMemory;
    VkImageView ImageView;
    VkSampler Sampler;
    bool bHasSampler = false;
    ImageUsage Usage;
    VkDescriptorImageInfo DescriptorInfo;
    VkFormat InnerFormat;

    RHIVulkanImageResource() = default;
    virtual ~RHIVulkanImageResource() override = default;

    virtual void Initialize(RHIContext* Context, const char* ImageFileName, RHIFormat InFormat, uint32_t MipLevel = -1) override;
    virtual void InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage = IU_COLOR_RT, uint32_t MultiSamplesCount = 1) override;
    virtual void Cleanup(RHIContext* Context) override;
};

class RHIVulkanBufferResource : public RHIBufferResourceBase
{
public:
    VkBuffer Buffer;
    VkDeviceMemory DeviceMemory;
    BufferType Type;

    RHIVulkanBufferResource() = default;
    virtual ~RHIVulkanBufferResource() override = default;

    virtual void Initialize(RHIContext* Context, uint32_t Stride, uint32_t ElementCounts, BufferType Type) override;
    virtual void CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes) override;
    virtual void Cleanup(RHIContext* Context) override;

    static VkBufferUsageFlags GetVkBufferUsageFlags(BufferType Type);
};

class RHIVulkanUniform : public RHIUniformBase
{
public:
    VkBuffer Buffer;
    VkDeviceMemory DeviceMemory;
    void* MappedMemory;
    VkDescriptorBufferInfo DescriptorBufferInfo;
    uint32_t Size;

    RHIVulkanUniform() = default;
    virtual ~RHIVulkanUniform() override = default;

    virtual void Initialize(RHIContext* Context, uint32_t UniformStructSize) override;
    virtual void CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes) override;
    virtual void Cleanup(RHIContext* Context) override;
};

class RHIVulkanRenderPass : public RHIRenderPassBase
{
public:
    VkExtent2D Extent;
    VkRenderPass RenderPass;
    std::vector<RHIVulkanImageResource*> ColorRenderTargets;
    RHIVulkanImageResource* DepthRenderTargets = nullptr;
    std::vector<VkAttachmentDescription> Attachments;
    int32_t DepthAttachmentIndex = -1;
    VkFramebuffer FrameBuffer;

    RHIVulkanRenderPass() = default;
    virtual ~RHIVulkanRenderPass() override = default;
    
    virtual void Initialize(RHIContext* Context, uint32_t Width, uint32_t Height) override;
    virtual void AddColorRenderTarget(RHIImageResource* ColorRT) override;
    virtual void SetDepthRenderTarget(RHIImageResource* DepthRT) override;
    virtual void Cleanup(RHIContext* Context) override;
};

class RHIVulkanPresentPass : public RHIPresentPassBase
{
public:
    VkRenderPass RenderPass;
    RHIVulkanImageResource* ColorRT;
    RHIVulkanImageResource* DepthRT;
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	std::vector<VkFramebuffer> SwapchainFramebuffers;
    RHIVulkanPresentPass();
    virtual ~RHIVulkanPresentPass() override;

    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples, RHIImageResource* ColorRT, RHIImageResource* DepthRT) override;
    virtual void CreateSwapchainFramebuffer(RHIContext* Context, RHIWindowManager* WindowManager) override;
    virtual void CleanupSwapchainFramebuffer(RHIContext* Context) override;
    virtual void Cleanup(RHIContext* Context) override;
    virtual void OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager) override;
};


class RHIVulkanPipeline : public RHIPipelineBase
{
    std::vector<char> VertShaderBytecode;
    std::vector<char> FragShaderBytecode;
public:
    VkDescriptorPool DescriptorPool;
    VkDescriptorSetLayout DescriptorSetLayout;
    VkPipeline Pipeline;
    VkPipelineLayout PipelineLayout;

    std::vector<VkVertexInputBindingDescription> BindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> AttributeDescriptions;

    VkDescriptorSet DescriptorSet;
    std::vector<VkDescriptorImageInfo> DescriptorImageInfos;
    std::vector<VkDescriptorBufferInfo> DescriptorBufferInfos;
    std::vector<VkDescriptorSetLayoutBinding> DescSetLayoutBindings;
    std::vector<VkWriteDescriptorSet> WriteDescriptorSets;
    uint32_t UniformBufferDescriptorCount = 0;
    uint32_t CombinedImageSamplerDescriptorCount = 0;

    RHIVulkanPipeline() = default;
    virtual ~RHIVulkanPipeline() override = default;

    virtual void AddLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset) override;
    virtual void AddBinding(uint32_t BindingIndex, uint32_t Stride) override;
    virtual void SetUniformBinding(RHIUniform* Uniform, uint32_t Binding) override;
    virtual void SetImageSamplerBinding(RHIImageResource* ImageResource, uint32_t Binding) override;
    virtual void SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader) override;
    virtual void Initialize(RHIContext* Context, RHIRenderPass* RenderPassResource) override;
    virtual void Initialize(RHIContext* Context, RHIPresentPass* PresentPass) override;
    virtual void Cleanup(RHIContext* Context) override;
};

class RHIVulkanImGUI : public RHIImGUIBase
{
public:
    ImGuiSharedGlobals ImGlobals;
    RHIVulkanImGUI() = default;
    virtual ~RHIVulkanImGUI() override = default;

    virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIPresentPass* PresentPass) override;
    virtual void DispatchImGUI(RHIGraphicsDispatcher* Dispatcher) override;
    virtual void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) override;
    virtual void Cleanup() override;
};

class RHIVulkanGraphicDispatcher : public RHIGraphicsDispatcherBase
{
public:
    bool bWindowResizeLastframe = false;
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
    uint32_t CurrentImageIndex;

    RHIVulkanGraphicDispatcher() = default;
    virtual ~RHIVulkanGraphicDispatcher() override = default;

    virtual void Initialize(RHIContext* Context) override;
    virtual void Cleanup(RHIContext* Context, RHIWindowManager* WindowManager) override;
    virtual void BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex) override;
    virtual void BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset) override;
    virtual void Dispatch(RHIWindowManager* WindowManager, RHIPipeline* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) override;
    virtual void BeginRenderPass(RHIContext* Context, RHIRenderPass* RenderPass) override;
    virtual void BeginPresentPass(RHIContext* Context, RHIWindowManager* WindowManager, RHIPresentPass* PresentPassResource) override;
    virtual void EndRenderPass(RHIRenderPass* RenderPass) override;
    virtual void EndPresentPassAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager) override;
    virtual void BeginFrame() override;
    virtual void WaitForGPUIdle(RHIContext* Context) override;
};
