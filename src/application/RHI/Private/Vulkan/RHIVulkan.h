// RHIVulkan.h - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
// @description RHI Vulkan implementation

#pragma once
#include <stdexcept>
#include <unordered_map>
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

	void Initialize() override;
	void Cleanup() override;
	void InitializePhysicalDevice(RHIWindowManager* WindowManager) override;


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

	// Helpers
	VkFormatProperties GetFormatProperties(VkFormat Format)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(CurrentPhysicalDevice.PhysicalDevice, Format, &props);
		return props;
	}

	VkFormat GetSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling,
	                            VkFormatFeatureFlags features)
	{
		for (VkFormat format : candidates)
		{
			auto props = GetFormatProperties(format);
			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
			{
				return format;
			}
			if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
			{
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}

	VkFormat GetDepthFormat()
	{
		return GetSupportedFormat(
			{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}


	VkSampleCountFlagBits GetMaxUsableSampleCount()
	{
		VkSampleCountFlags counts = CurrentPhysicalDevice.PDProperties.limits.framebufferColorSampleCounts &
			CurrentPhysicalDevice.PDProperties.limits.framebufferDepthSampleCounts;
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
		for (uint32_t i = 0; i < CurrentPhysicalDevice.PDMemoryProperties.memoryTypeCount; i++)
		{
			if ((TypeFilter & (1 << i)) && (CurrentPhysicalDevice.PDMemoryProperties.memoryTypes[i].propertyFlags &
				Properties) == Properties)
			{
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
		switch (InFormat)
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
	~RHIVulkanContext() override;

	void Initialize(RHIPlatformSupport* PlatformSupport) override;
	void Cleanup() override;
	void WaitDeviceIdle() override;
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
	~RHIVulkanWindowManager() override = default;

	void Initialize(RHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth) override;
	void Cleanup(RHIPlatformSupport* PlatformSupport) override;

	void InitializeSwapchain(RHIContext* Context, RHIPlatformSupport* PlatformSupport) override;
	void CleanupSwapchain(RHIContext* Context) override;
	void RecreateSwapchain(RHIContext* Context) override;
	bool IsAlive() override;

	uint32_t GetWindowHeight() override
	{
		return CurrentSwapchain.SwapchainExtent.height;
	}

	uint32_t GetWindowWidth() override
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
	~RHIVulkanImageResource() override = default;

	void Initialize(RHIContext* Context, const char* ImageFileName, RHIFormat InFormat,
	                uint32_t MipLevel = -1) override;
    void Initialize(RHIContext* Context, void* Data, uint32_t Size, uint32_t Height, uint32_t Width, RHIFormat InFormat, uint32_t MipLevel = -1) override;
	void InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent,
	                            ImageUsage InUsage = IU_COLOR_RT, uint32_t MultiSamplesCount = 1) override;
	void Cleanup(RHIContext* Context) override;
};

class RHIVulkanBufferResource : public RHIBufferResourceBase
{
public:
	VkBuffer Buffer;
	VkDeviceMemory DeviceMemory;
	BufferType Type;

	RHIVulkanBufferResource() = default;
	~RHIVulkanBufferResource() override = default;

	void Initialize(RHIContext* Context, uint32_t Stride, uint32_t ElementCounts, BufferType Type) override;
	void CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes) override;
	void Cleanup(RHIContext* Context) override;

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
	~RHIVulkanUniform() override = default;

	void Initialize(RHIContext* Context, uint32_t UniformStructSize) override;
	void CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes) override;
	void Cleanup(RHIContext* Context) override;
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
	~RHIVulkanRenderPass() override = default;

	void Initialize(RHIContext* Context, uint32_t Width, uint32_t Height) override;
	void AddColorRenderTarget(RHIImageResource* ColorRT) override;
	void SetDepthRenderTarget(RHIImageResource* DepthRT) override;
	void Cleanup(RHIContext* Context) override;
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
	~RHIVulkanPresentPass() override;

	void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples,
	                RHIImageResource* ColorRT, RHIImageResource* DepthRT) override;
	void CreateSwapchainFramebuffer(RHIContext* Context, RHIWindowManager* WindowManager) override;
	void CleanupSwapchainFramebuffer(RHIContext* Context) override;
	void Cleanup(RHIContext* Context) override;
	void OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager) override;
};


class RHIVulkanPipelineFactory : public RHIPipelineFactoryBase
{
	std::vector<char> VertShaderBytecode;
	std::vector<char> FragShaderBytecode;

public:
	std::vector<VkVertexInputBindingDescription> BindingDescriptions;
	std::vector<VkVertexInputAttributeDescription> AttributeDescriptions;

	//std::unordered_map<uint32_t, VkVertexInputBindingDescription> BindingDescMap;
	//std::unordered_map<std::pair<uint32_t, uint32_t>, VkVertexInputAttributeDescription> AttributeDescMap;

	std::vector<VkDescriptorImageInfo> DescriptorImageInfos;
	std::vector<VkDescriptorBufferInfo> DescriptorBufferInfos;
	std::vector<VkDescriptorSetLayoutBinding> DescSetLayoutBindings;

	uint32_t UniformBufferDescriptorCount = 0;
	uint32_t CombinedImageSamplerDescriptorCount = 0;
	RHIVulkanPipelineFactory() = default;
	~RHIVulkanPipelineFactory() override = default;

	void AddBufferLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset) override;
	void AddBufferBinding(uint32_t BindingIndex, uint32_t Stride) override;
    void RemoveAllBufferBindings() override;
	void SetUniformBinding(uint32_t Binding) override;
	void SetImageSamplerBinding(uint32_t Binding) override;
	void RemoveAllGlobalBindings() override;
	void SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader) override;
	void InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context,
	                              RHIRenderPass* RenderPassResource) override;
	void InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context,
	                              RHIPresentPass* PresentPass) override;
	void Cleanup(RHIContext* Context) override;
};

class RHIVulkanPipelineObject : public RHIPipelineObjectBase
{
public:
	VkPipeline Pipeline;
	VkPipelineLayout PipelineLayout;
	std::vector<VkWriteDescriptorSet> WriteDescriptorSets;
	VkDescriptorSetLayout DescriptorSetLayout;
	RHIVulkanPipelineObject() = default;
	~RHIVulkanPipelineObject() override = default;

	void SetUniform(RHIUniform* Uniform, uint32_t Binding) override;
	void SetImageSampler(RHIImageResource* ImageResource, uint32_t Binding) override;
	void Cleanup(RHIContext* Context) override;
};

class RHIVulkanImGUI : public RHIImGUIBase
{
public:
	ImGuiSharedGlobals ImGlobals;
	RHIVulkanImGUI() = default;
	~RHIVulkanImGUI() override = default;

	void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIPresentPass* PresentPass) override;
	void DispatchImGUI(RHIGraphicsDispatcher* Dispatcher) override;
	void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) override;
	void Cleanup() override;
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
	~RHIVulkanGraphicDispatcher() override = default;

	void Initialize(RHIContext* Context) override;
	void Cleanup(RHIContext* Context, RHIWindowManager* WindowManager) override;
	void BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex) override;
	void BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset) override;
	void Dispatch(RHIWindowManager* WindowManager, RHIPipelineObject* PipelineObject, uint32_t IndexCount,
	              uint32_t IndexOffset, uint32_t InstanceCount) override;
	void BeginRenderPass(RHIContext* Context, RHIRenderPass* RenderPass) override;
	void BeginPresentPass(RHIContext* Context, RHIWindowManager* WindowManager,
	                      RHIPresentPass* PresentPassResource) override;
	void EndRenderPass(RHIRenderPass* RenderPass) override;
	void EndPresentPassAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager) override;
	void BeginFrame() override;
	void WaitForGPUIdle(RHIContext* Context) override;
};
