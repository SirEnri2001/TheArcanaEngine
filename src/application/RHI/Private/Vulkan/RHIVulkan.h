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
class VkCommandBufferManaged;
struct GLFWwindow;
struct ImDrawData;


class RHIVulkanCommandBuffer;
class RHIVulkanContext;
class RHIVulkanFrameBuffer;
class RHIVulkanImageResource;
class RHIVulkanImGUI;
class RHIVulkanPipelineFactory;
class RHIVulkanPipelineObject;
class RHIVulkanPlatformSupport;
class RHIVulkanRenderPass;
class RHIVulkanSwapchain;
class RHIVulkanBuffer;

struct VkImageStateDesc
{
	VkPipelineStageFlags PipelineStage;
	VkAccessFlags Access;
	VkImageLayout ImageLayout;
};

class RHIVulkanPlatformSupport : public IRHIPlatformSupport
{
public:
	RHIVulkanPlatformSupport();

	void Initialize() override;
	void Cleanup() override;
	std::unique_ptr<IRHIContext> CreateRHIContext() override;

	static VkFormat GetVkFormat(RHIFormat InFormat);
	static VkDescriptorType GetDescriptorType(DescriptorType Type);
	static VkImageStateDesc GetStateDesc(RHIResourceState InState);
};

class RHIVulkanContext : public IRHIContext
{
public:
	VkDevice Device = VK_NULL_HANDLE;
	VkQueue GraphicsQueue;
	VkQueue PresentQueue;
	VkCommandPool FrameCommandPool;
	VkCommandPool TransientCommandPool;

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

	GLFWwindow* pGLFWwindow;
	VkSurfaceKHR Surface;
	VkSurfaceCapabilitiesKHR SurfaceCapabilities;
	std::vector<VkSurfaceFormatKHR> SurfaceFormats;
	std::vector<VkPresentModeKHR> PresentModes;
	std::vector<RHIVulkanImageResource*> ScreenSizeImages;

	RHIVulkanContext();
	~RHIVulkanContext() override;

	void Initialize(uint32_t WindowWidth, uint32_t WindowHeight) override;
	void Cleanup() override;
	void WaitDeviceIdle() override;
	void ProcessFrameInput() override;
	bool IsWindowAlive() override;
	virtual std::unique_ptr<IRHICommandBuffer     > CreateRHICommandBuffer      () override;
	virtual std::unique_ptr<IRHIFrameBuffer       > CreateRHIFrameBuffer        () override;
	virtual std::unique_ptr<IRHIImageResource     > CreateRHIImageResource      () override;
    virtual std::unique_ptr<IRHIImGUI             > CreateRHIImGUI              () override;
    virtual std::unique_ptr<IRHIPipelineFactory   > CreateRHIPipelineFactory    () override;
    virtual std::unique_ptr<IRHIPipelineObject    > CreateRHIPipelineObject     () override;
	virtual std::unique_ptr<IRHIRenderPass        > CreateRHIRenderPass         () override;
    virtual std::unique_ptr<IRHISwapchain         > CreateRHISwapchain          () override;
    virtual std::unique_ptr<IRHIBuffer           > CreateRHIBuffer            () override;
	static void OnWindowResize(GLFWwindow* window, int width, int height);
	bool QuerySurfaceProperties();

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
};

class RHIVulkanImageResource;

class RHIVulkanSwapchain : public IRHISwapchain
{
public:
	VkSwapchainKHR Swapchain;
	VkExtent2D SwapchainExtent;
	std::vector<IRHIFrameBuffer*> SwapchainFrameBuffers;
	std::vector<VkImage> SwapchainImages;
	std::vector<VkImageView> SwapchainImageViews;
	VkFormat SwapchainImageFormat;
	RHIFormat SwapchainRHIFormat;
	std::unique_ptr<RHIVulkanImageResource> DepthImageResource;
	VkRenderPass CachedRenderPass; // framebuffer for this specified renderpass
	uint32_t CurrentImageIndex;

	VkSemaphore ImageAvailableSemaphore;
	VkSemaphore RenderFinishSemaphore;
	VkSemaphore TransitionFinishSemaphore;
	VkFence InFlightFence;

	bool SwapchainAvailable = false;

	RHIVulkanSwapchain();
	RHIVulkanSwapchain(const RHIVulkanSwapchain&) = delete;
	virtual ~RHIVulkanSwapchain();
	virtual void Initialize(IRHIContext* Context, RHIFormat InSwapchainFormat) override;
	virtual void Cleanup(IRHIContext* Context) override;
	virtual void AcquireFrame(IRHIContext* Context, IRHIFrameBuffer*& OutFrameBuffer, IRHIRenderPass* InRenderPass) override;
	virtual void PresentFrameAndRelease(IRHIContext* Context, IRHICommandBuffer* CommandBuffer) override;
	virtual ImageExtent3D GetFrameSize() override;
	virtual RHIFormat GetFrameFormat() override
	{
		return SwapchainRHIFormat;
	}
	void Recreate(RHIVulkanContext* Context);

private:
	void CreateFramebuffers(RHIVulkanContext* Context, VkRenderPass VkRP);
	void DestroyFramebuffers(RHIVulkanContext* Context);
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
	void CopyToTexture(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, void* Data, uint32_t Stride) override;
	void Cleanup(IRHIContext* Context) override;
	VkDescriptorImageInfo& GetDescriptorImageInfo();
	virtual void Resize(IRHIContext* Context, uint32_t Height, uint32_t Width) override;
	virtual void Transition(IRHICommandBuffer* CommandBuffer, RHIResourceState InState) override;
};

class RHIVulkanBuffer : public IRHIBuffer
{
public:
	VkBuffer Buffer;
	VkDeviceMemory DeviceMemory;
	void* MappedMemory;
	VkDescriptorBufferInfo DescriptorBufferInfo;
	uint32_t Size;

	RHIVulkanBuffer() = default;
	~RHIVulkanBuffer() override = default;

	void Initialize(IRHIContext* Context, uint32_t BufferSize, RHIResourceState InState) override;
    virtual void Initialize(IRHIContext* Context, uint32_t ElementSize, uint32_t NumElements, RHIResourceState InState) override {
		return Initialize(Context, ElementSize * NumElements, InState);
	}
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
	virtual void Cleanup(IRHIContext* Context) override;
	virtual void BeginRenderPass(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer* Framebuffer) override;
	virtual void EndRenderPass(IRHICommandBuffer* CommandBuffer) override;
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
	void SetStorageBufferBinding(uint32_t Binding) override;
	void SetImageSamplerBinding(uint32_t Binding) override;
	void SetDescriptorBinding(uint32_t BindingIndex, DescriptorType BindingDescriptorType) override;
	void RemoveAllGlobalBindings() override;
	void SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader) override;
	void SetComputeShaders(const std::vector<char>& ComputeShader) override;
	void InitializePipelineObject(IRHIPipelineObject* OutPipelineObject, IRHIContext* Context,
	                              IRHIRenderPass* RenderPassResource) override;
	void InitializeComputePipelineObject(IRHIPipelineObject* OutComputePipelineObject, IRHIContext* Context) override;
	void Cleanup(IRHIContext* Context) override;
};

class RHIVulkanPipelineObject : public IRHIPipelineObject
{
public:
	struct BindingInfo
	{
		VkBuffer Buffer;
		VkDeviceSize Offset;
		uint32_t BindingIndex;
	};

	std::vector<BindingInfo> BindingInfos;
	BindingInfo IndexBindingInfo;

	VkPipeline Pipeline;
	VkPipelineLayout PipelineLayout;
	std::vector<VkWriteDescriptorSet> WriteDescriptorSets;
	VkDescriptorSetLayout DescriptorSetLayout;
	RHIVulkanPipelineObject() = default;
	~RHIVulkanPipelineObject() override = default;

	void SetUniform(IRHIBuffer* Uniform, uint32_t Binding) override;
	void SetStorageBuffer(IRHIBuffer* StorageBuffer, uint32_t Binding) override;
	void SetImageSampler(IRHIImageResource* ImageResource, uint32_t Binding) override;
	void SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, IRHIImageResource* ImageResource) override;
	void SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, IRHIBuffer* Uniform) override;
	virtual void BindVertexBuffer(IRHIBuffer* Buffer, uint32_t Offset, uint32_t BindingIndex) override;
	virtual void BindIndexBuffer(IRHIBuffer* Buffer, uint32_t Offset) override;
	void Cleanup(IRHIContext* Context) override;
	virtual void CopyDescriptors(IRHIContext* Context) override {}

	void Draw(IRHICommandBuffer* CommandBuffer, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) override;
	void Dispatch(IRHICommandBuffer* CommandBuffer, uint32_t ThreadGroupX, uint32_t ThreadGroupY, uint32_t ThreadGroupZ) override;
};

class RHIVulkanImGUI : public IRHIImGUI
{
public:
	ImGuiSharedGlobals ImGlobals;
	bool initialized = false;
	RHIVulkanImGUI() = default;
	~RHIVulkanImGUI() override = default;

	void Initialize(IRHIContext* Context, IRHISwapchain* Swapchain, IRHIRenderPass* PresentRenderPass) override;
	void DispatchImGUI(IRHICommandBuffer* CommandBuffer) override;
	void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) override;
	void Cleanup() override;
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