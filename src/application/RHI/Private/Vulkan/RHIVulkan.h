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
class VkCommandBufferManaged;
struct GLFWwindow;
struct ImDrawData;

struct VkImageStateDesc
{
	VkPipelineStageFlags PipelineStage;
	VkAccessFlags Access;
	VkImageLayout ImageLayout;
};

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
		case B8G8R8A8_SRGB:
			return VK_FORMAT_B8G8R8A8_SRGB;
		case R32G32B32_SFLOAT:
			return VK_FORMAT_R32G32B32_SFLOAT;
		case R32G32_SFLOAT:
			return VK_FORMAT_R32G32_SFLOAT;
		case R32G32B32A32_SFLOAT:
			return VK_FORMAT_R32G32B32A32_SFLOAT;
		case D32_SFLOAT:
			return VK_FORMAT_D32_SFLOAT;
		}
		return VK_FORMAT_UNDEFINED;
	}

	static VkDescriptorType GetDescriptorType(DescriptorType Type)
	{
		switch (Type)
		{
		case UNIFORM:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case SAMPLER2D:
			return VK_DESCRIPTOR_TYPE_SAMPLER;
		case IMAGE2D:
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
		return VK_DESCRIPTOR_TYPE_SAMPLER;
	}
	/*
	static VkImageLayout GetImageLayout(ImageUsage InUsage)
	{
		switch (InUsage)
		{
		case IU_GENERAL:
		case IU_STORAGE:
			return VkImageLayout::VK_IMAGE_LAYOUT_GENERAL;
		case IU_COLOR_RT:
		case IU_COLOR_PRESENT_RT:
			return VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		case IU_DEPTH_RT:
		case IU_DEPTH_PRESENT_RT:
			return VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		case IU_SHADER_READ:
			return VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}
		return VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;
	}
	*/
	static VkImageStateDesc GetStateDesc(RHIResourceState InState)
	{
		VkImageStateDesc StateDesc{};
		// Principle: wide stage bits, narrow access bits - by ChatGPT
		switch (InState)
		{
		case RHIResourceState::UNDEFINED:
			StateDesc.ImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			StateDesc.Access = 0;
			StateDesc.PipelineStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;
		case RHIResourceState::SHADER_READ:
			StateDesc.ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			StateDesc.Access = VK_ACCESS_SHADER_READ_BIT;
			StateDesc.PipelineStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			break;
		case RHIResourceState::SHADER_WRITE:
			StateDesc.ImageLayout = VK_IMAGE_LAYOUT_GENERAL;
			StateDesc.Access = VK_ACCESS_SHADER_WRITE_BIT;
			StateDesc.PipelineStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
			break;
		case RHIResourceState::SHADER_READWRITE:
			break;
		case RHIResourceState::COLOR_ATTACHMENT:
			StateDesc.ImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			StateDesc.Access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			StateDesc.PipelineStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;
		case RHIResourceState::DEPTH_ATTACHMENT:
			StateDesc.ImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			StateDesc.Access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			StateDesc.PipelineStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;
		case RHIResourceState::COPY_SRC:
			break;
		case RHIResourceState::COPY_DST:
			StateDesc.ImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			StateDesc.Access = VK_ACCESS_TRANSFER_WRITE_BIT;
			StateDesc.PipelineStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case RHIResourceState::PRESENTABLE:
			StateDesc.ImageLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			StateDesc.Access = VK_ACCESS_MEMORY_READ_BIT;
			StateDesc.PipelineStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;
		default:
			break;
		}
		return StateDesc;
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

class RHIVulkanImageResource;

class RHIVulkanWindowManager : public RHIWindowManagerBase
{
public:
	RHIPlatformSupport* PlatformSupport;
	
	GLFWwindow* pGLFWwindow;
	VkSurfaceKHR Surface;
	VkSurfaceCapabilitiesKHR SurfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> SurfaceFormats;
	std::vector<VkPresentModeKHR> PresentModes;
	std::vector<RHIVulkanImageResource*> ScreenSizeImages;

	RHIVulkanWindowManager() = default;
	~RHIVulkanWindowManager() override = default;

	void Initialize(RHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth) override;
	void Cleanup(RHIPlatformSupport* PlatformSupport) override;

	void InitializeSwapchain(RHIContext* Context, RHIPlatformSupport* PlatformSupport) override;
	void CleanupSwapchain(RHIContext* Context) override;
	void RecreateSwapchain(RHIContext* Context) override;
	void AddScreenSizeTexture(RHIImageResource* ImageResource) override;
	void RemoveScreenSizeTexture(RHIImageResource* ImageResource) override;
	void InitializeRenderPassAsPresent(RHIRenderPass* OutRenderPass, RHIContext* Context) override;
	bool IsAlive() override;

	virtual RHIImageResource* GetColorRenderTarget() override {
		return nullptr;
	}
	virtual RHIImageResource* GetDepthRenderTarget() override {
		return nullptr;
	}
	static void OnWindowResize(GLFWwindow* window, int width, int height);
};


class RHIVulkanSwapchain : public RHISwapchainBase
{
public:
	VkSwapchainKHR Swapchain;
	VkExtent2D SwapchainExtent;
	std::vector<RHIFrameBuffer*> SwapchainFrameBuffers;
	std::vector<VkImage> SwapchainImages;
	std::vector<VkImageView> SwapchainImageViews;
	VkFormat SwapchainImageFormat;
	RHIVulkanImageResource* DepthImageResource;
	VkRenderPass CachedRenderPass; // framebuffer for this specified renderpass
	uint32_t CurrentImageIndex;

	VkSemaphore ImageAvailableSemaphore;
	VkSemaphore RenderFinishSemaphore;
	VkFence InFlightFence;

	RHIVulkanSwapchain();
	RHIVulkanSwapchain(const RHIVulkanSwapchain&) = delete;
	virtual ~RHIVulkanSwapchain();
	virtual void Initialize(RHIContext* Context, RHIWindowManager* WindowManager) override;
	virtual void Cleanup(RHIContext* Context) override;
	virtual void AcquireFrame(RHIContext* Context, RHIFrameBuffer*& OutFrameBuffer, RHIRenderPass* InRenderPass) override;
	virtual void PresentFrameAndRelease(RHIContext* Context, RHICommandBuffer* CommandBuffer, RHIGraphicsDispatcher* GDispatcher) override;
	virtual ImageExtent3D GetFrameSize() override;

private:
	void CreateFramebuffers(RHIVulkanContext* Context, VkRenderPass VkRP);
};

class RHIVulkanImageResource : public RHIImageResourceBase
{
public:
	VkImage Image;
	VkDeviceMemory DeviceMemory;
	VkImageStateDesc Desc;
	VkDescriptorImageInfo DescriptorInfo;
	bool bHasSampler = false;
	//ImageUsage Usage;
	VkFormat InnerFormat;
	VkExtent3D ImageExtent;
	uint32_t MipLevel = -1;

	RHIVulkanImageResource() = default;
	~RHIVulkanImageResource() override = default;

    void Initialize(RHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t InMipLevel = -1) override;
	void Initialize(RHIContext* Context, ImageExtent3D RTExtent, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t MipLevel) override;
	void InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent,
	                            RHIResourceState InUsage, uint32_t MultiSamplesCount = 1) override;
    void CopyToTexture(RHICommandBuffer* CommandBuffer, RHIContext* Context, void* Data, uint32_t Stride) override;
	void Cleanup(RHIContext* Context) override;
	VkDescriptorImageInfo& GetDescriptorImageInfo();
	virtual void Resize(RHIContext* Context, uint32_t Height, uint32_t Width) override;
	virtual void Transition(RHICommandBuffer* CommandBuffer, RHIResourceState InState) override;
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
	void CopyToBuffer(RHICommandBuffer* CommandBuffer, RHIContext* Context, void* data, uint32_t TotalBytes) override;
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
	VkRenderPass RenderPass;
	std::vector<VkAttachmentDescription> Attachments;
	int32_t DepthAttachmentIndex = -1;

	RHIVulkanRenderPass() = default;
	~RHIVulkanRenderPass() override = default;

	virtual void Initialize(RHIContext* Context, std::vector<RHIFormat> ColorRTFormats, bool hasDepth, RHIFormat DepthFormat) override;
	void Cleanup(RHIContext* Context) override;
};

class RHIVulkanPresentPass : public RHIPresentPassBase
{
public:
	VkRenderPass RenderPass;
	RHIVulkanImageResource* ColorRT;
	RHIVulkanImageResource* DepthRT;
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	RHIVulkanPresentPass();
	~RHIVulkanPresentPass() override;

	void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples,
	                RHIImageResource* ColorRT, RHIImageResource* DepthRT) override;
	void Cleanup(RHIContext* Context) override;
	void OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager) override;
};


class RHIVulkanPipelineFactory : public RHIPipelineFactoryBase
{
	std::vector<char> VertShaderBytecode;
	std::vector<char> FragShaderBytecode;
	std::vector<char> ComputeShaderBytecode;
	bool bCompute = false;
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
	void SetDescriptorBinding(uint32_t BindingIndex, DescriptorType BindingDescriptorType) override;
	void RemoveAllGlobalBindings() override;
	void SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader) override;
	void SetComputeShaders(const std::vector<char>& ComputeShader) override;
	void InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context,
	                              RHIRenderPass* RenderPassResource) override;
	void InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context,
	                              RHIPresentPass* PresentPass) override;
	void InitializeComputePipelineObject(RHIPipelineObject* OutComputePipelineObject, RHIContext* Context) override;
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
	void SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, RHIImageResource* ImageResource) override;
	void SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, RHIUniform* Uniform) override;
	void Cleanup(RHIContext* Context) override;
};

class RHIVulkanImGUI : public RHIImGUIBase
{
public:
	ImGuiSharedGlobals ImGlobals;
	bool initialized = false;
	RHIVulkanImGUI() = default;
	~RHIVulkanImGUI() override = default;

	void Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHISwapchain* Swapchain, RHIRenderPass* PresentRenderPass) override;
	void DispatchImGUI(RHICommandBuffer* CommandBuffer, RHIGraphicsDispatcher* Dispatcher) override;
	void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) override;
	void Cleanup() override;
};

class RHIVulkanGraphicDispatcher : public RHIGraphicsDispatcherBase
{
public:
	bool bWindowResizeLastframe = false;

	struct BindingInfo
	{
		RHIVulkanBufferResource* BufferResource;
		VkDeviceSize Offset;
		uint32_t BindingIndex;
	};

	std::vector<BindingInfo> BindingInfos;
	BindingInfo IndexBindingInfo;

	RHIVulkanGraphicDispatcher() = default;
	~RHIVulkanGraphicDispatcher() override = default;

	void Initialize(RHIContext* Context) override;
	void Cleanup(RHIContext* Context, RHIWindowManager* WindowManager) override;
	void BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex) override;
	void BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset) override;
	void Draw(RHICommandBuffer* CommandBuffer, RHIPipelineObject* PipelineObject, uint32_t IndexCount,
	              uint32_t IndexOffset, uint32_t InstanceCount) override;
	void BeginRenderPass(RHICommandBuffer* CommandBuffer, RHIRenderPass* RenderPass, RHIFrameBuffer* Framebuffer) override;
	void EndRenderPass(RHICommandBuffer* CommandBuffer, RHIRenderPass* RenderPass) override;
	void BeginFrame(RHICommandBuffer* CommandBuffer, RHIContext* Context, RHISwapchain* Swapchain, RHIRenderPass* PresentRenderPass) override;
	void EndFrameAndSubmit(RHICommandBuffer* CommandBuffer, RHIContext* Context, RHIWindowManager* WindowManager, RHIFrameBuffer* PresentFrameBuffer = nullptr) override;
	void WaitForGPUIdle(RHIContext* Context) override;
	virtual void TransitionImageAsShaderRead(RHICommandBuffer* CommandBuffer, RHIImageResource* Image) override;
	virtual void TransitionImageAsRenderTarget(RHICommandBuffer* CommandBuffer, RHIImageResource* Image) override;
};

class RHIVulkanFrameBuffer : public RHIFrameBufferBase {
public:
	VkFramebuffer FrameBuffer;
	VkExtent3D Extent;
	RHIVulkanFrameBuffer();
	RHIVulkanFrameBuffer(const RHIVulkanFrameBuffer&) = delete;
	virtual ~RHIVulkanFrameBuffer() override;
	virtual void Initialize(RHIContext* Context, RHIRenderPass* RenderPass,
		std::vector<RHIImageResource*> ColorRTs, RHIImageResource* DepthRT) override;
	virtual void Cleanup(RHIContext* Context) override;
};

class RHIVulkanCommandBuffer : public RHICommandBufferBase
{
public:
	VkCommandBuffer CommandBuffer;
	VkDevice Device;
	VkCommandPool CommandPool;
	RHIVulkanCommandBuffer() = default;
	RHIVulkanCommandBuffer(const RHIVulkanCommandBuffer&) = delete;
	virtual ~RHIVulkanCommandBuffer() override;

	virtual void Initialize(RHIContext* Context) override;
	virtual void Cleanup(RHIContext* Context) override;

	virtual void BeginCommandBuffer() override;
	virtual void EndCommandBuffer() override;
	virtual void ResetCommandBuffer() override;
};

class RHIVulkanComputeDispatcher : public RHIComputeDispatcherBase
{
public:
	RHIVulkanComputeDispatcher() = default;
	~RHIVulkanComputeDispatcher() override = default;

	void Initialize(RHIContext* Context) override;
	void Cleanup(RHIContext* Context) override;
	void Dispatch(RHICommandBuffer* CommandBuffer, RHIPipelineObject* PipelineObject, uint32_t ThreadGroupX, uint32_t ThreadGroupY, uint32_t ThreadGroupZ) override;
	void WaitForGPUIdle(RHIContext* Context) override;
};