// RHIVulkan.h - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
// @description RHI Vulkan implementation

#pragma once
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <memory>

#include "vulkan/vulkan_core.h"

#include "RHI.h"
#include "RHIImGuiHelper.h"

// Forward declarations
class RHIVulkanWindowManager;
class VkCommandBufferManaged;
struct GLFWwindow;
struct ImDrawData;


class RHIVulkanBufferResource;
class RHIVulkanCommandBuffer;
class RHIVulkanComputeDispatcher;
class RHIVulkanContext;
class RHIVulkanFrameBuffer;
class RHIVulkanGraphicsDispatcher;
class RHIVulkanImageResource;
class RHIVulkanImGUI;
class RHIVulkanPipelineFactory;
class RHIVulkanPipelineObject;
class RHIVulkanPlatformSupport;
class RHIVulkanPresentPass;
class RHIVulkanRenderPass;
class RHIVulkanSwapchain;
class RHIVulkanUniform;
class RHIVulkanWindowManager;

struct VkImageStateDesc
{
	VkPipelineStageFlags PipelineStage;
	VkAccessFlags Access;
	VkImageLayout ImageLayout;
};

class RHIVulkanPlatformSupport : public IRHIPlatformSupport
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
	std::unique_ptr<IRHIContext> CreateRHIContext() override;


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

class RHIVulkanContext : public IRHIContext
{
public:
	VkDevice Device = VK_NULL_HANDLE;
	VkQueue GraphicsQueue;
	VkQueue PresentQueue;
	VkCommandPool CommandPool;

	RHIVulkanContext();
	~RHIVulkanContext() override;

	void Initialize(IRHIPlatformSupport* PlatformSupport) override;
	void Cleanup() override;
	void WaitDeviceIdle() override;
	virtual std::unique_ptr<IRHIBufferResource    > CreateRHIBufferResource     () override;
    virtual std::unique_ptr<IRHICommandBuffer     > CreateRHICommandBuffer      () override;
    virtual std::unique_ptr<IRHIComputeDispatcher > CreateRHIComputeDispatcher  () override;
    virtual std::unique_ptr<IRHIFrameBuffer       > CreateRHIFrameBuffer        () override;
    virtual std::unique_ptr<IRHIGraphicsDispatcher> CreateRHIGraphicsDispatcher () override;
    virtual std::unique_ptr<IRHIImageResource     > CreateRHIImageResource      () override;
    virtual std::unique_ptr<IRHIImGUI             > CreateRHIImGUI              () override;
    virtual std::unique_ptr<IRHIPipelineFactory   > CreateRHIPipelineFactory    () override;
    virtual std::unique_ptr<IRHIPipelineObject    > CreateRHIPipelineObject     () override;
    virtual std::unique_ptr<IRHIPresentPass       > CreateRHIPresentPass        () override;
    virtual std::unique_ptr<IRHIRenderPass        > CreateRHIRenderPass         () override;
    virtual std::unique_ptr<IRHISwapchain         > CreateRHISwapchain          () override;
    virtual std::unique_ptr<IRHIUniform           > CreateRHIUniform            () override;
    virtual std::unique_ptr<IRHIWindowManager     > CreateRHIWindowManager      () override;
};

class RHIVulkanImageResource;

class RHIVulkanWindowManager : public IRHIWindowManager
{
public:
	IRHIPlatformSupport* PlatformSupport;
	
	GLFWwindow* pGLFWwindow;
	VkSurfaceKHR Surface;
	VkSurfaceCapabilitiesKHR SurfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> SurfaceFormats;
	std::vector<VkPresentModeKHR> PresentModes;
	std::vector<RHIVulkanImageResource*> ScreenSizeImages;

	RHIVulkanWindowManager() = default;
	~RHIVulkanWindowManager() override = default;

	void Initialize(IRHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth) override;
	void Cleanup(IRHIPlatformSupport* PlatformSupport) override;

	void InitializeSwapchain(IRHIContext* Context, IRHIPlatformSupport* PlatformSupport) override;
	void CleanupSwapchain(IRHIContext* Context) override;
	void RecreateSwapchain(IRHIContext* Context) override;
	void AddScreenSizeTexture(IRHIImageResource* ImageResource) override;
	void RemoveScreenSizeTexture(IRHIImageResource* ImageResource) override;
	void InitializeRenderPassAsPresent(IRHIRenderPass* OutRenderPass, IRHIContext* Context) override;
	bool IsAlive() override;

	virtual IRHIImageResource* GetColorRenderTarget() override {
		return nullptr;
	}
	virtual IRHIImageResource* GetDepthRenderTarget() override {
		return nullptr;
	}
	static void OnWindowResize(GLFWwindow* window, int width, int height);
};


class RHIVulkanSwapchain : public IRHISwapchain
{
public:
	VkSwapchainKHR Swapchain;
	VkExtent2D SwapchainExtent;
	std::vector<IRHIFrameBuffer*> SwapchainFrameBuffers;
	std::vector<VkImage> SwapchainImages;
	std::vector<VkImageView> SwapchainImageViews;
	VkFormat SwapchainImageFormat;
	RHIVulkanImageResource* DepthImageResource;
	VkRenderPass CachedRenderPass; // framebuffer for this specified renderpass
	uint32_t CurrentImageIndex;

	VkSemaphore ImageAvailableSemaphore;
	VkSemaphore RenderFinishSemaphore;
	VkSemaphore TransitionFinishSemaphore;
	VkFence InFlightFence;

	RHIVulkanSwapchain();
	RHIVulkanSwapchain(const RHIVulkanSwapchain&) = delete;
	virtual ~RHIVulkanSwapchain();
	virtual void Initialize(IRHIContext* Context, IRHIWindowManager* WindowManager) override;
	virtual void Cleanup(IRHIContext* Context) override;
	virtual void AcquireFrame(IRHIContext* Context, IRHIFrameBuffer*& OutFrameBuffer, IRHIRenderPass* InRenderPass) override;
	virtual void PresentFrameAndRelease(IRHIContext* Context, IRHICommandBuffer* CommandBuffer) override;
	virtual ImageExtent3D GetFrameSize() override;

private:
	void CreateFramebuffers(RHIVulkanContext* Context, VkRenderPass VkRP);
};

class RHIVulkanImageResource : public IRHIImageResource
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

    void Initialize(IRHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t InMipLevel = -1) override;
	void Initialize(IRHIContext* Context, ImageExtent3D RTExtent, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t MipLevel) override;
	void InitializeRenderTarget(IRHIContext* Context, IRHIWindowManager* WindowManager, ImageExtent3D RTExtent,
	                            RHIResourceState InUsage, uint32_t MultiSamplesCount = 1) override;
    void CopyToTexture(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, void* Data, uint32_t Stride) override;
	void Cleanup(IRHIContext* Context) override;
	VkDescriptorImageInfo& GetDescriptorImageInfo();
	virtual void Resize(IRHIContext* Context, uint32_t Height, uint32_t Width) override;
	virtual void Transition(IRHICommandBuffer* CommandBuffer, RHIResourceState InState) override;
};

class RHIVulkanBufferResource : public IRHIBufferResource
{
public:
	VkBuffer Buffer;
	VkDeviceMemory DeviceMemory;
	BufferType Type;

	RHIVulkanBufferResource() = default;
	~RHIVulkanBufferResource() override = default;

	void Initialize(IRHIContext* Context, uint32_t Stride, uint32_t ElementCounts, BufferType Type) override;
	void CopyToBuffer(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, void* data, uint32_t TotalBytes) override;
	void Cleanup(IRHIContext* Context) override;

	static VkBufferUsageFlags GetVkBufferUsageFlags(BufferType Type);
};

class RHIVulkanUniform : public IRHIUniform
{
public:
	VkBuffer Buffer;
	VkDeviceMemory DeviceMemory;
	void* MappedMemory;
	VkDescriptorBufferInfo DescriptorBufferInfo;
	uint32_t Size;

	RHIVulkanUniform() = default;
	~RHIVulkanUniform() override = default;

	void Initialize(IRHIContext* Context, uint32_t UniformStructSize) override;
	void CopyToBuffer(IRHIContext* Context, void* data, uint32_t TotalBytes) override;
	void Cleanup(IRHIContext* Context) override;
};

class RHIVulkanRenderPass : public IRHIRenderPass
{
public:
	VkRenderPass RenderPass;
	std::vector<VkAttachmentDescription> Attachments;
	int32_t DepthAttachmentIndex = -1;

	RHIVulkanRenderPass() = default;
	~RHIVulkanRenderPass() override = default;

	virtual void Initialize(IRHIContext* Context, std::vector<RHIFormat> ColorRTFormats, bool hasDepth, RHIFormat DepthFormat) override;
	void Cleanup(IRHIContext* Context) override;
};

class RHIVulkanPresentPass : public IRHIPresentPass
{
public:
	VkRenderPass RenderPass;
	RHIVulkanImageResource* ColorRT;
	RHIVulkanImageResource* DepthRT;
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	RHIVulkanPresentPass();
	~RHIVulkanPresentPass() override;

	void Initialize(IRHIContext* Context, IRHIWindowManager* WindowManager, uint32_t MSAASamples,
	                IRHIImageResource* ColorRT, IRHIImageResource* DepthRT) override;
	void Cleanup(IRHIContext* Context) override;
	void OnWindowResize(IRHIContext* Context, IRHIWindowManager* WindowManager) override;
};


class RHIVulkanPipelineFactory : public IRHIPipelineFactory
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
	void InitializePipelineObject(IRHIPipelineObject* OutPipelineObject, IRHIContext* Context,
	                              IRHIRenderPass* RenderPassResource) override;
	void InitializePipelineObject(IRHIPipelineObject* OutPipelineObject, IRHIContext* Context,
	                              IRHIPresentPass* PresentPass) override;
	void InitializeComputePipelineObject(IRHIPipelineObject* OutComputePipelineObject, IRHIContext* Context) override;
	void Cleanup(IRHIContext* Context) override;
};

class RHIVulkanPipelineObject : public IRHIPipelineObject
{
public:
	VkPipeline Pipeline;
	VkPipelineLayout PipelineLayout;
	std::vector<VkWriteDescriptorSet> WriteDescriptorSets;
	VkDescriptorSetLayout DescriptorSetLayout;
	RHIVulkanPipelineObject() = default;
	~RHIVulkanPipelineObject() override = default;

	void SetUniform(IRHIUniform* Uniform, uint32_t Binding) override;
	void SetImageSampler(IRHIImageResource* ImageResource, uint32_t Binding) override;
	void SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, IRHIImageResource* ImageResource) override;
	void SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, IRHIUniform* Uniform) override;
	void Cleanup(IRHIContext* Context) override;
};

class RHIVulkanImGUI : public IRHIImGUI
{
public:
	ImGuiSharedGlobals ImGlobals;
	bool initialized = false;
	RHIVulkanImGUI() = default;
	~RHIVulkanImGUI() override = default;

	void Initialize(IRHIContext* Context, IRHIWindowManager* WindowManager, IRHISwapchain* Swapchain, IRHIRenderPass* PresentRenderPass) override;
	void DispatchImGUI(IRHICommandBuffer* CommandBuffer, IRHIGraphicsDispatcher* Dispatcher) override;
	void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) override;
	void Cleanup() override;
};

class RHIVulkanGraphicsDispatcher : public IRHIGraphicsDispatcher
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

	RHIVulkanGraphicsDispatcher() = default;
	~RHIVulkanGraphicsDispatcher() override = default;

	void Initialize(IRHIContext* Context) override;
	void Cleanup(IRHIContext* Context, IRHIWindowManager* WindowManager) override;
	void BindVertexBuffer(IRHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex) override;
	void BindIndexBuffer(IRHIBufferResource* BufferResource, uint32_t Offset) override;
	void Draw(IRHICommandBuffer* CommandBuffer, IRHIPipelineObject* PipelineObject, uint32_t IndexCount,
	              uint32_t IndexOffset, uint32_t InstanceCount) override;
	void BeginRenderPass(IRHICommandBuffer* CommandBuffer, IRHIRenderPass* RenderPass, IRHIFrameBuffer* Framebuffer) override;
	void EndRenderPass(IRHICommandBuffer* CommandBuffer, IRHIRenderPass* RenderPass) override;
	void BeginFrame(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, IRHISwapchain* Swapchain, IRHIRenderPass* PresentRenderPass) override;
	void EndFrameAndSubmit(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, IRHIWindowManager* WindowManager, IRHIFrameBuffer* PresentFrameBuffer = nullptr) override;
	void WaitForGPUIdle(IRHIContext* Context) override;
	virtual void TransitionImageAsShaderRead(IRHICommandBuffer* CommandBuffer, IRHIImageResource* Image) override;
	virtual void TransitionImageAsRenderTarget(IRHICommandBuffer* CommandBuffer, IRHIImageResource* Image) override;
};

class RHIVulkanFrameBuffer : public IRHIFrameBuffer {
public:
	VkFramebuffer FrameBuffer;
	VkExtent3D Extent;
	RHIVulkanFrameBuffer();
	RHIVulkanFrameBuffer(const RHIVulkanFrameBuffer&) = delete;
	virtual ~RHIVulkanFrameBuffer() override;
	virtual void Initialize(IRHIContext* Context, IRHIRenderPass* RenderPass,
		std::vector<IRHIImageResource*> ColorRTs, IRHIImageResource* DepthRT) override;
	virtual void Cleanup(IRHIContext* Context) override;
};

class RHIVulkanCommandBuffer : public IRHICommandBuffer
{
public:
	VkCommandBuffer CommandBuffer;
	VkDevice Device;
	VkCommandPool CommandPool;
	RHIVulkanCommandBuffer() = default;
	RHIVulkanCommandBuffer(const RHIVulkanCommandBuffer&) = delete;
	virtual ~RHIVulkanCommandBuffer() override;

	virtual void Initialize(IRHIContext* Context) override;
	virtual void Cleanup(IRHIContext* Context) override;

	virtual void BeginCommandBuffer() override;
	virtual void EndCommandBuffer() override;
	virtual void ResetCommandBuffer() override;
};

class RHIVulkanComputeDispatcher : public IRHIComputeDispatcher
{
public:
	RHIVulkanComputeDispatcher() = default;
	~RHIVulkanComputeDispatcher() override = default;

	void Initialize(IRHIContext* Context) override;
	void Cleanup(IRHIContext* Context) override;
	void Dispatch(IRHICommandBuffer* CommandBuffer, IRHIPipelineObject* PipelineObject, uint32_t ThreadGroupX, uint32_t ThreadGroupY, uint32_t ThreadGroupZ) override;
	void WaitForGPUIdle(IRHIContext* Context) override;
};