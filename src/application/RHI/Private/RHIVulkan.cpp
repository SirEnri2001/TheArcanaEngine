#define RHI_IMPLEMENT
#include "RHI.h"
#include "RHIVulkan.h"

#include <stdexcept>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <array>
#include <cassert>
#include <iostream>
#include <optional>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <stb_image.h>

#include "RHIVulkanImpl.h"
#include "GLFW/glfw3.h"
#define GLM_ENABLE_EXPERIMENTAL 
#include "glm/gtx/compatibility.hpp"

RHIVulkanPlatformSupport::RHIVulkanPlatformSupport() {}

RHIVulkanPlatformSupport* RHIVulkanPlatformSupport::GInstance = nullptr;

RHIVulkanPlatformSupport* RHIVulkanPlatformSupport::Get()
{
	return reinterpret_cast<RHIVulkanPlatformSupport*>(RHIPlatformSupport::Get()->GetImpl());
}


void RHIVulkanPlatformSupport::Initialize()
{
	CreateGLFWContext();
	VkDebugUtilsMessengerCreateInfoEXT DebugCreateInfo{};
    DebugCreateInfo.flags = 0;
    DebugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    DebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    DebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    DebugCreateInfo.pfnUserCallback = debugCallback;
	CreateVkInstance(Instance, DebugCreateInfo, DebugMessenger, InstanceExtensions);
	

    if (vkCreateDebugUtilsMessengerEXT(Instance, &DebugCreateInfo, nullptr, &DebugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
	PhysicalDeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
	};
	RetrieveAvailablePhysicalDevices(AvailablePhysicalDevices, Instance, PhysicalDeviceExtensions);
}

void RHIVulkanPlatformSupport::Cleanup()
{
	vkDestroyInstance(Instance, nullptr);
}

void RHIVulkanPlatformSupport::InitializePhysicalDevice(RHIWindowManager* WindowManager)
{
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	assert(CurrentPhysicalDevice.PhysicalDevice==nullptr);
	// Select suitable physical device
	for (auto& PD : AvailablePhysicalDevices)
	{
		VulkanWindowManager->SurfaceFormats.clear();
		VulkanWindowManager->PresentModes.clear();
		vkGetPhysicalDeviceFeatures(PD, &CurrentPhysicalDevice.PDFeatures);
		if (PhysicalDeviceSupportSurface(VulkanWindowManager->SurfaceCapabilities, VulkanWindowManager->SurfaceFormats, VulkanWindowManager->PresentModes, PD, VulkanWindowManager->Surface)
			&& PhysicalDevideSupportQueueFamilies(CurrentPhysicalDevice.GraphicsQueueFamilyIndex, CurrentPhysicalDevice.PresentQueueFamilyIndex, PD, VulkanWindowManager->Surface)
			&& CurrentPhysicalDevice.PDFeatures.samplerAnisotropy)
		{
			CurrentPhysicalDevice.PhysicalDevice = PD;
			break;
		}
	}

	if (CurrentPhysicalDevice.PhysicalDevice == VK_NULL_HANDLE)
	{
		throw std::runtime_error("failed to find a suitable GPU!");
	}

	vkGetPhysicalDeviceProperties(CurrentPhysicalDevice.PhysicalDevice, &CurrentPhysicalDevice.PDProperties);
	vkGetPhysicalDeviceMemoryProperties(CurrentPhysicalDevice.PhysicalDevice, &CurrentPhysicalDevice.PDMemoryProperties);
}

RHIVulkanContext::RHIVulkanContext() {}
RHIVulkanContext::~RHIVulkanContext() {}

void RHIVulkanContext::Initialize(RHIPlatformSupport* PlatformSupport)
{
	auto* VulkanPlatformSupport = static_cast<RHIVulkanPlatformSupport*>(PlatformSupport->GetImpl());
	assert(VulkanPlatformSupport->CurrentPhysicalDevice.PhysicalDevice != nullptr);
	CreateVkDevice(Device, GraphicsQueue, PresentQueue, VulkanPlatformSupport->CurrentPhysicalDevice.PhysicalDevice, 
		VulkanPlatformSupport->CurrentPhysicalDevice.GraphicsQueueFamilyIndex, VulkanPlatformSupport->CurrentPhysicalDevice.PresentQueueFamilyIndex, VulkanPlatformSupport->PhysicalDeviceExtensions);
	CreateCommandPool(CommandPool, Device, VulkanPlatformSupport->CurrentPhysicalDevice.GraphicsQueueFamilyIndex);
}

void RHIVulkanContext::Cleanup()
{
	vkDestroyCommandPool(Device, CommandPool, nullptr);
	vkDestroyDevice(Device, nullptr);

	//if (enableValidationLayers) {
	//    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	//}
}

void RHIVulkanContext::WaitDeviceIdle()
{
	vkDeviceWaitIdle(Device);
}

void RHIVulkanWindowManager::Initialize(RHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth)
{
	auto* VulkanPlatformSupport = static_cast<RHIVulkanPlatformSupport*>(PlatformSupport->GetImpl());
	CreateGLFWWindow(pGLFWwindow, WindowWidth, WindowHeight, this, OnWindowResize);
	CreateVkSurface(VulkanPlatformSupport->Instance, pGLFWwindow, Surface);
}

void RHIVulkanWindowManager::CleanupSwapchain(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	for (auto imageView : CurrentSwapchain.SwapchainImageViews) {
		vkDestroyImageView(VulkanContext->Device, imageView, nullptr);
	}
	vkDestroySwapchainKHR(VulkanContext->Device, CurrentSwapchain.Swapchain, nullptr);
}

void RHIVulkanWindowManager::InitializeSwapchain(RHIContext* Context, RHIPlatformSupport* PlatformSupport)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanPlatformSupport = static_cast<RHIVulkanPlatformSupport*>(PlatformSupport->GetImpl());
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VulkanPlatformSupport->CurrentPhysicalDevice.PhysicalDevice, Surface, &SurfaceCapabilities);
	glfwGetFramebufferSize(pGLFWwindow, reinterpret_cast<int*>(&CurrentSwapchain.SwapchainExtent.width), reinterpret_cast<int*>(&CurrentSwapchain.SwapchainExtent.height));
	while (CurrentSwapchain.SwapchainExtent.width == 0 || CurrentSwapchain.SwapchainExtent.height == 0) {
		glfwGetFramebufferSize(pGLFWwindow, reinterpret_cast<int*>(&CurrentSwapchain.SwapchainExtent.width), reinterpret_cast<int*>(&CurrentSwapchain.SwapchainExtent.height));
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(VulkanContext->Device);
	CreateSwapchain(
		CurrentSwapchain.Swapchain, CurrentSwapchain.SwapchainImages, CurrentSwapchain.SwapchainImageViews, CurrentSwapchain.SwapchainImageFormat, CurrentSwapchain.SwapchainExtent,
		VulkanContext->Device, Surface, SurfaceCapabilities, SurfaceFormats, PresentModes, 
		VulkanPlatformSupport->CurrentPhysicalDevice.GraphicsQueueFamilyIndex, VulkanPlatformSupport->CurrentPhysicalDevice.PresentQueueFamilyIndex);
}

void RHIVulkanWindowManager::OnWindowResize(GLFWwindow* window, int width, int height)
{
	auto self = reinterpret_cast<RHIVulkanContext*>(glfwGetWindowUserPointer(window));
	std::cout<<"Windows Resize!\n";
}

void RHIVulkanWindowManager::Cleanup(RHIPlatformSupport* PlatformSupport)
{
	auto* VulkanPlatformSupport = static_cast<RHIVulkanPlatformSupport*>(PlatformSupport->GetImpl());
	vkDestroySurfaceKHR(VulkanPlatformSupport->Instance, Surface, nullptr);
	glfwDestroyWindow(pGLFWwindow);
	glfwTerminate();
}

bool RHIVulkanWindowManager::IsAlive()
{
	return !glfwWindowShouldClose(pGLFWwindow);
}

void RHIVulkanImageResource::InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage, uint32_t MultiSamplesCount)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	VkFormat InVkFormat;
	VkImageUsageFlags VkImageUsage;
	VkImageAspectFlagBits VkImageAspectFlagBits;
	Usage = InUsage;
	switch (Usage)
	{
	case IU_COLOR_RT:
		InVkFormat = VulkanWindowManager->CurrentSwapchain.SwapchainImageFormat;
		VkImageUsage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT;
		break;
	case IU_DEPTH_RT:
		InVkFormat = RHIVulkanPlatformSupport::Get()->GetDepthFormat();
		VkImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	default:
		throw std::runtime_error("Invalid usage bit for init a rendertarget");
	}
	VkExtent3D vkExtent;
	vkExtent.height = RTExtent.Height;
	vkExtent.width = RTExtent.Width;
	vkExtent.depth = RTExtent.Depth;
	CreateImage(Image, VulkanContext->Device, vkExtent, 1, static_cast<VkSampleCountFlagBits>(MultiSamplesCount), InVkFormat, VK_IMAGE_TILING_OPTIMAL, VkImageUsage);
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(VulkanContext->Device, Image, &memRequirements);
	CreateDeviceMemory(DeviceMemory, VulkanContext->Device, memRequirements.size, RHIVulkanPlatformSupport::Get()->GetMemoryType(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	vkBindImageMemory(VulkanContext->Device, Image, DeviceMemory, 0);
	CreateImageView(ImageView, Image, VulkanContext->Device, InVkFormat, VkImageAspectFlagBits, 1);
}


void RHIVulkanImageResource::Initialize(RHIContext* Context, const char* ImageFileName, RHIFormat InFormat, uint32_t MipLevel)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	VkFormat InVkFormat;
	Usage = IU_GENERAL;
	InVkFormat = RHIVulkanPlatformSupport::GetVkFormat(InFormat);
	// Check if image format supports linear blitting
	VkFormatProperties formatProperties = RHIVulkanPlatformSupport::Get()->GetFormatProperties(InVkFormat);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(ImageFileName, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	assert(texHeight > 0 && texWidth > 0);
	VkDeviceSize imageSize = texWidth * texHeight * 4;
	if(MipLevel==-1)
	{
		MipLevel = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
	}

	if (!pixels || texWidth<=0 || texHeight<=0) {
		throw std::runtime_error("failed to load texture image!");
	}
	
	VkMemoryRequirements memRequirements;
	VkExtent3D ImageExtent;

	{
		ImageExtent.height = texHeight;
		ImageExtent.width = texWidth;
		ImageExtent.depth = 1;
		CreateImage(Image, VulkanContext->Device, ImageExtent, MipLevel, VK_SAMPLE_COUNT_1_BIT, InVkFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		vkGetImageMemoryRequirements(VulkanContext->Device, Image, &memRequirements);
		CreateDeviceMemory(DeviceMemory, VulkanContext->Device, memRequirements.size, RHIVulkanPlatformSupport::Get()->GetMemoryType(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
		vkBindImageMemory(VulkanContext->Device, Image, DeviceMemory, 0);
		CreateImageView(ImageView, Image, VulkanContext->Device, InVkFormat, VK_IMAGE_ASPECT_COLOR_BIT, MipLevel);
		CreateSampler(Sampler, VulkanContext->Device, RHIVulkanPlatformSupport::Get()->CurrentPhysicalDevice.PDProperties.limits.maxSamplerAnisotropy);
	}
    
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	{
		CreateBuffer(stagingBuffer, VulkanContext->Device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	    vkGetBufferMemoryRequirements(VulkanContext->Device, stagingBuffer, &memRequirements);
		CreateDeviceMemory(stagingBufferMemory, VulkanContext->Device, memRequirements.size, RHIVulkanPlatformSupport::Get()->GetMemoryType(memRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
		vkBindBufferMemory(VulkanContext->Device, stagingBuffer, stagingBufferMemory, 0);

		void* data;
		vkMapMemory(VulkanContext->Device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(VulkanContext->Device, stagingBufferMemory);

		stbi_image_free(pixels);
	}

	VkCommandBuffer commandBuffer;
	CreateCommandBuffer(commandBuffer, VulkanContext->Device, VulkanContext->CommandPool);
	BeginCommandBufferOneTimeSubmit(commandBuffer, VulkanContext->CommandPool, VulkanContext->Device);
	TransitionImageLayout(Image, commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, MipLevel);
	CopyBufferToImage(stagingBuffer, Image, commandBuffer, ImageExtent.width, ImageExtent.height);
	CreateMipmapForImage(commandBuffer, Image, texWidth, texHeight, MipLevel);
	EndCommandBufferOneTimeSubmit(commandBuffer, VulkanContext->CommandPool, VulkanContext->GraphicsQueue, VulkanContext->Device);

	vkDestroyBuffer(VulkanContext->Device, stagingBuffer, nullptr);
	vkFreeMemory(VulkanContext->Device, stagingBufferMemory, nullptr);
	bHasSampler = true;
}

void RHIVulkanImageResource::Cleanup(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	vkDestroyImage(VulkanContext->Device, Image, nullptr);
	vkDestroyImageView(VulkanContext->Device, ImageView, nullptr);
	vkFreeMemory(VulkanContext->Device, DeviceMemory, nullptr);
	if (bHasSampler)
	{
		vkDestroySampler(VulkanContext->Device, Sampler, nullptr);
	}
}

VkBufferUsageFlags RHIVulkanBufferResource::GetVkBufferUsageFlags(BufferType Type)
{
	switch (Type)
	{
	case VERTEX:
		return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	case INDEX:
		return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	case GENERAL:
	default:
		return 0;
	}
}


void RHIVulkanBufferResource::Initialize(RHIContext* Context, uint32_t Stride, uint32_t ElementCounts, BufferType InType)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	Type = InType;
	CreateBuffer(Buffer, VulkanContext->Device, Stride * ElementCounts, GetVkBufferUsageFlags(Type) | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	VkMemoryRequirements MemRequirements;
	vkGetBufferMemoryRequirements(VulkanContext->Device, Buffer, &MemRequirements);
	CreateDeviceMemory(DeviceMemory, VulkanContext->Device, MemRequirements.size, RHIVulkanPlatformSupport::Get()->GetMemoryType(MemRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	vkBindBufferMemory(VulkanContext->Device, Buffer, DeviceMemory, 0);
}

void RHIVulkanBufferResource::CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CreateBuffer(stagingBuffer, VulkanContext->Device, TotalBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	VkMemoryRequirements MemRequirement;
	vkGetBufferMemoryRequirements(VulkanContext->Device, stagingBuffer, &MemRequirement);
	CreateDeviceMemory(stagingBufferMemory, VulkanContext->Device, MemRequirement.size, RHIVulkanPlatformSupport::Get()->GetMemoryType(MemRequirement, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	vkBindBufferMemory(VulkanContext->Device, stagingBuffer, stagingBufferMemory, 0);
	void* pMappedBuffer;
	vkMapMemory(VulkanContext->Device, stagingBufferMemory, 0, TotalBytes, 0, &pMappedBuffer);
	memcpy(pMappedBuffer, data, TotalBytes);
	vkUnmapMemory(VulkanContext->Device, stagingBufferMemory);
	VkCommandBuffer CommandBuffer;
	CreateCommandBuffer(CommandBuffer, VulkanContext->Device, VulkanContext->CommandPool);
	BeginCommandBufferOneTimeSubmit(CommandBuffer, VulkanContext->CommandPool, VulkanContext->Device);
	CopyBuffer(stagingBuffer, Buffer, TotalBytes, CommandBuffer);
	EndCommandBufferOneTimeSubmit(CommandBuffer, VulkanContext->CommandPool, VulkanContext->GraphicsQueue, VulkanContext->Device);

}

void RHIVulkanBufferResource::Cleanup(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	vkDestroyBuffer(VulkanContext->Device, Buffer, nullptr);
	vkFreeMemory(VulkanContext->Device, DeviceMemory, nullptr);
}

void RHIVulkanUniform::Initialize(RHIContext* Context, uint32_t UniformStructSize)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	CreateBuffer(Buffer, VulkanContext->Device, UniformStructSize,	VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	VkMemoryRequirements MemRequirements;
	vkGetBufferMemoryRequirements(VulkanContext->Device, Buffer, &MemRequirements);
	CreateDeviceMemory(DeviceMemory, VulkanContext->Device, MemRequirements.size, RHIVulkanPlatformSupport::Get()->GetMemoryType(MemRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	vkBindBufferMemory(VulkanContext->Device, Buffer, DeviceMemory, 0);
	vkMapMemory(VulkanContext->Device, DeviceMemory, 0, UniformStructSize, 0, &MappedMemory);
	DescriptorBufferInfo = {};
	DescriptorBufferInfo.buffer = Buffer;
	DescriptorBufferInfo.offset = 0;
	DescriptorBufferInfo.range = UniformStructSize;
	Size = UniformStructSize;
}

void RHIVulkanUniform::CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	memcpy(MappedMemory, data, TotalBytes);
}

void RHIVulkanUniform::Cleanup(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	vkUnmapMemory(VulkanContext->Device, DeviceMemory);
	vkDestroyBuffer(VulkanContext->Device, Buffer, nullptr);
	vkFreeMemory(VulkanContext->Device, DeviceMemory, nullptr);
}

void RHIVulkanPipeline::AddLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset)
{
	AttributeDescriptions.push_back({ Location, BindingIndex, RHIVulkanPlatformSupport::GetVkFormat(Format), Offset });
}

void RHIVulkanPipeline::AddBinding(uint32_t BindingIndex, uint32_t Stride)
{
	BindingDescriptions.push_back({ BindingIndex, Stride, VK_VERTEX_INPUT_RATE_VERTEX });
}

void RHIVulkanPipeline::AddUniformBuffer(RHIUniform* Uniform, uint32_t Binding)
{
	auto* VulkanUniform = static_cast<RHIVulkanUniform*>(Uniform->GetImpl());
	VkWriteDescriptorSet WriteDescSet{};
	WriteDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	WriteDescSet.dstSet = nullptr;
	WriteDescSet.dstBinding = Binding;
	WriteDescSet.dstArrayElement = 0;
	WriteDescSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	WriteDescSet.descriptorCount = 1;
	DescriptorBufferInfos.push_back({VulkanUniform->Buffer, 0u, VulkanUniform->Size});
	WriteDescSet.pBufferInfo = &DescriptorBufferInfos.back();
	WriteDescriptorSets.push_back(WriteDescSet);
	UniformBufferDescriptorCount++;

	VkDescriptorSetLayoutBinding uboLayoutBinding;
    uboLayoutBinding.binding = Binding;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	DescSetLayoutBindings.push_back(uboLayoutBinding);
}

void RHIVulkanPipeline::AddImageSampler(RHIImageResource* ImageResource, uint32_t Binding)
{
	auto* VulkanImageResource = static_cast<RHIVulkanImageResource*>(ImageResource->GetImpl());
	VkWriteDescriptorSet WriteDescSet{};
	WriteDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	WriteDescSet.dstSet = nullptr;
	WriteDescSet.dstBinding = Binding;
	WriteDescSet.dstArrayElement = 0;
	WriteDescSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	WriteDescSet.descriptorCount = 1;
	DescriptorImageInfos.push_back({VulkanImageResource->Sampler, VulkanImageResource->ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
	WriteDescSet.pImageInfo = &DescriptorImageInfos.back();
	WriteDescriptorSets.push_back(WriteDescSet);
	CombinedImageSamplerDescriptorCount++;

    VkDescriptorSetLayoutBinding samplerLayoutBinding;
    samplerLayoutBinding.binding = Binding;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	DescSetLayoutBindings.push_back(samplerLayoutBinding);
}


void RHIVulkanPipeline::SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader)
{
	VertShaderBytecode = VertShader;
	FragShaderBytecode = FragShader;
}


void RHIVulkanPipeline::Initialize(RHIContext* Context, RHIRenderPass* RenderPassResource)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanRenderPassResource = static_cast<RHIVulkanRenderPass*>(RenderPassResource->GetImpl());
	if (UniformBufferDescriptorCount>0 || CombinedImageSamplerDescriptorCount > 0)
	{
		CreateDescriptorSetLayout(DescriptorSetLayout, DescSetLayoutBindings, VulkanContext->Device);
		//CreateDescriptorPool(DescriptorPool, VulkanContext->Device);
		//CreateDescriptorSet(DescriptorSet, WriteDescriptorSets, VulkanContext->Device, DescriptorPool, DescriptorSetLayout);
	}
	else
	{
		DescriptorSetLayout = nullptr;
		DescriptorPool = nullptr;
		DescriptorSet = nullptr;
	}
	CreateGraphicsPipeline(Pipeline, PipelineLayout, VulkanRenderPassResource->msaaSamples, VulkanContext->Device, VertShaderBytecode, "main", FragShaderBytecode, "main",
		BindingDescriptions, AttributeDescriptions, DescriptorSetLayout, VulkanRenderPassResource->RenderPass);
}


void RHIVulkanPipeline::Cleanup(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	vkDestroyPipeline(VulkanContext->Device, Pipeline, nullptr);
	vkDestroyPipelineLayout(VulkanContext->Device, PipelineLayout, nullptr);
	vkDestroyDescriptorPool(VulkanContext->Device, DescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(VulkanContext->Device, DescriptorSetLayout, nullptr);
}

void RHIVulkanRenderPass::Initialize(RHIContext* Context, RHIWindowManager* WindowManager)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	msaaSamples = RHIVulkanPlatformSupport::Get()->GetMaxUsableSampleCount();
	CreateRenderPass(RenderPass, VulkanContext->Device, RHIVulkanPlatformSupport::Get()->GetDepthFormat(), VulkanWindowManager->CurrentSwapchain.SwapchainImageFormat, msaaSamples);
	ImageExtent3D RenderTargetExtent;
	RenderTargetExtent.Height = VulkanWindowManager->CurrentSwapchain.SwapchainExtent.height;
	RenderTargetExtent.Width = VulkanWindowManager->CurrentSwapchain.SwapchainExtent.width;
	RenderTargetExtent.Depth = 1;
	ColorRenderTargetResource.InitializeRenderTarget(Context, WindowManager, RenderTargetExtent, IU_COLOR_RT, msaaSamples);
	DepthRenderTargetResource.InitializeRenderTarget(Context, WindowManager, RenderTargetExtent, IU_DEPTH_RT, msaaSamples);
	CreateSwapchainFramebuffer(Context, WindowManager);
}

void RHIVulkanRenderPass::CreateSwapchainFramebuffer(RHIContext* Context, RHIWindowManager* WindowManager)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	SwapchainFramebuffers.resize(VulkanWindowManager->CurrentSwapchain.SwapchainImageViews.size());
	for (size_t i = 0; i < VulkanWindowManager->CurrentSwapchain.SwapchainImageViews.size(); i++) {
		std::array<VkImageView, 3> attachments = {
			ColorRenderTargetResource.ImageView,
			DepthRenderTargetResource.ImageView,
			VulkanWindowManager->CurrentSwapchain.SwapchainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = RenderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = VulkanWindowManager->CurrentSwapchain.SwapchainExtent.width;
		framebufferInfo.height = VulkanWindowManager->CurrentSwapchain.SwapchainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(VulkanContext->Device, &framebufferInfo, nullptr, &SwapchainFramebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}
void RHIVulkanRenderPass::CleanupSwapchainFramebuffer(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	for (auto framebuffer : SwapchainFramebuffers) {
		vkDestroyFramebuffer(VulkanContext->Device, framebuffer, nullptr);
	}
}

void RHIVulkanRenderPass::Cleanup(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	CleanupSwapchainFramebuffer(Context);
	// clean up
	ColorRenderTargetResource.Cleanup(Context);
	DepthRenderTargetResource.Cleanup(Context);
	vkDestroyRenderPass(VulkanContext->Device, RenderPass, nullptr);
}


void RHIVulkanGraphicDispatcher::Initialize(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	CreateCommandBuffer(CommandBuffer, VulkanContext->Device, VulkanContext->CommandPool);
	if (vkCreateSemaphore(VulkanContext->Device, &semaphoreInfo, nullptr, &ImageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(VulkanContext->Device, &semaphoreInfo, nullptr, &RenderFinishSemaphore) != VK_SUCCESS ||
		vkCreateFence(VulkanContext->Device, &fenceInfo, nullptr, &InFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("failed to create synchronization objects for a frame!");
	}
}

void RHIVulkanGraphicDispatcher::Cleanup(RHIContext* Context, RHIWindowManager* WindowManager)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	for (int i = 0; i < VulkanWindowManager->CurrentSwapchain.SwapchainImageViews.size(); i++)
	{
		vkDestroySemaphore(VulkanContext->Device, ImageAvailableSemaphore, nullptr);
		vkDestroySemaphore(VulkanContext->Device, RenderFinishSemaphore, nullptr);
		vkDestroyFence(VulkanContext->Device, InFlightFence, nullptr);
	}
}

void RHIVulkanGraphicDispatcher::BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex)
{
	auto* VulkanBufferResource = static_cast<RHIVulkanBufferResource*>(BufferResource->GetImpl());
	BindingInfo Info;
	Info.BindingIndex = BindingIndex;
	Info.BufferResource = VulkanBufferResource;
	Info.Offset = Offset;
	BindingInfos.push_back(Info);
}

void RHIVulkanGraphicDispatcher::BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset)
{
	auto* VulkanBufferResource = static_cast<RHIVulkanBufferResource*>(BufferResource->GetImpl());
	IndexBindingInfo.BufferResource = VulkanBufferResource;
	IndexBindingInfo.Offset = Offset;
}

void RHIVulkanGraphicDispatcher::Dispatch(RHIWindowManager* WindowManager, RHIPipeline* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount)
{
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	auto* VulkanPipeline = static_cast<RHIVulkanPipeline*>(Pipeline->GetImpl());
	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipeline->Pipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)VulkanWindowManager->CurrentSwapchain.SwapchainExtent.width;
	viewport.height = (float)VulkanWindowManager->CurrentSwapchain.SwapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(CommandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = VulkanWindowManager->CurrentSwapchain.SwapchainExtent;
	vkCmdSetScissor(CommandBuffer, 0, 1, &scissor);

	for (auto& Info : BindingInfos)
	{
		vkCmdBindVertexBuffers(CommandBuffer, Info.BindingIndex, 1, &Info.BufferResource->Buffer, &Info.Offset);
	}
	vkCmdBindIndexBuffer(CommandBuffer, IndexBindingInfo.BufferResource->Buffer, IndexBindingInfo.Offset, VK_INDEX_TYPE_UINT32);
	vkCmdPushDescriptorSetKHR(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipeline->PipelineLayout,
		0, VulkanPipeline->WriteDescriptorSets.size(), VulkanPipeline->WriteDescriptorSets.data());
	//if (VulkanPipeline->DescriptorSet)
	//{
	//	
	//	//vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipeline->PipelineLayout, 0, 1,
	//	//	&VulkanPipeline->DescriptorSet, 0, nullptr);
	//}
	
	vkCmdDrawIndexed(CommandBuffer, IndexCount, InstanceCount, IndexOffset, 0, 0);
}

void RHIVulkanGraphicDispatcher::PrepareRenderPass(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* RenderPass, uint32_t& OutImageIndex)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	auto* VulkanRenderPass = static_cast<RHIVulkanRenderPass*>(RenderPass->GetImpl());
	glfwPollEvents();
	vkWaitForFences(VulkanContext->Device, 1, &InFlightFence, VK_TRUE, UINT64_MAX);
	if (bWindowResizeLastframe)
	{
		VulkanWindowManager->CleanupSwapchain(Context);
		VulkanRenderPass->CleanupSwapchainFramebuffer(Context);
		VulkanRenderPass->ColorRenderTargetResource.Cleanup(Context);
		VulkanRenderPass->DepthRenderTargetResource.Cleanup(Context);
		VulkanWindowManager->InitializeSwapchain(Context, RHIPlatformSupport::Get());
		ImageExtent3D RenderTargetExtent;
		RenderTargetExtent.Height = VulkanWindowManager->CurrentSwapchain.SwapchainExtent.height;
		RenderTargetExtent.Width = VulkanWindowManager->CurrentSwapchain.SwapchainExtent.width;
		RenderTargetExtent.Depth = 1;
		VulkanRenderPass->ColorRenderTargetResource.InitializeRenderTarget(Context, WindowManager, RenderTargetExtent, IU_COLOR_RT, VulkanRenderPass->msaaSamples);
		VulkanRenderPass->DepthRenderTargetResource.InitializeRenderTarget(Context, WindowManager, RenderTargetExtent, IU_DEPTH_RT, VulkanRenderPass->msaaSamples);
		VulkanRenderPass->CreateSwapchainFramebuffer(Context, WindowManager);
		bWindowResizeLastframe = false;
	}
	VkResult result = vkAcquireNextImageKHR(VulkanContext->Device, VulkanWindowManager->CurrentSwapchain.Swapchain, UINT64_MAX, ImageAvailableSemaphore, VK_NULL_HANDLE, &OutImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		VulkanWindowManager->CleanupSwapchain(Context);
		VulkanRenderPass->CleanupSwapchainFramebuffer(Context);
		VulkanRenderPass->ColorRenderTargetResource.Cleanup(Context);
		VulkanRenderPass->DepthRenderTargetResource.Cleanup(Context);
		VulkanWindowManager->InitializeSwapchain(Context, RHIPlatformSupport::Get());
		ImageExtent3D RenderTargetExtent;
		RenderTargetExtent.Height = VulkanWindowManager->CurrentSwapchain.SwapchainExtent.height;
		RenderTargetExtent.Width = VulkanWindowManager->CurrentSwapchain.SwapchainExtent.width;
		RenderTargetExtent.Depth = 1;
		VulkanRenderPass->ColorRenderTargetResource.InitializeRenderTarget(Context, WindowManager, RenderTargetExtent);
		VulkanRenderPass->DepthRenderTargetResource.InitializeRenderTarget(Context, WindowManager, RenderTargetExtent);
		VulkanRenderPass->CreateSwapchainFramebuffer(Context, WindowManager);
		bWindowResizeLastframe = false;
		VkResult result = vkAcquireNextImageKHR(VulkanContext->Device, VulkanWindowManager->CurrentSwapchain.Swapchain, UINT64_MAX, ImageAvailableSemaphore, VK_NULL_HANDLE, &OutImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain after retry!");
		}
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	vkResetFences(VulkanContext->Device, 1, &InFlightFence);

	vkResetCommandBuffer(CommandBuffer, /*VkCommandBufferResetFlagBits*/ 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(CommandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}
}


void RHIVulkanGraphicDispatcher::BeginRenderPass(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* RenderPassResource, uint32_t InImageIndex)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	auto* VulkanRenderPassResource = static_cast<RHIVulkanRenderPass*>(RenderPassResource->GetImpl());
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = VulkanRenderPassResource->RenderPass;
	renderPassInfo.framebuffer = VulkanRenderPassResource->SwapchainFramebuffers[InImageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = VulkanWindowManager->CurrentSwapchain.SwapchainExtent;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { {1.0f, 0.0f, 1.0f, 1.0f} };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void RHIVulkanGraphicDispatcher::EndRenderPass()
{
	vkCmdEndRenderPass(CommandBuffer);
}

void RHIVulkanGraphicDispatcher::Submit(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t ImageIndex)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &ImageAvailableSemaphore;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &CommandBuffer;

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &RenderFinishSemaphore;

	if (vkQueueSubmit(VulkanContext->GraphicsQueue, 1, &submitInfo, InFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &RenderFinishSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &VulkanWindowManager->CurrentSwapchain.Swapchain;
	presentInfo.pImageIndices = &ImageIndex;

	auto result = vkQueuePresentKHR(VulkanContext->PresentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		bWindowResizeLastframe = true;
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}
}

void RHIVulkanImGUI::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* RenderPass)
{
	auto VulkanPlatform = reinterpret_cast<RHIVulkanPlatformSupport*>(RHIPlatformSupport::Get()->GetImpl());
	auto vkWindow = reinterpret_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	auto vkContext = reinterpret_cast<RHIVulkanContext*>(Context->GetImpl());
	auto vkRenderPass = reinterpret_cast<RHIVulkanRenderPass*>(RenderPass->GetImpl());
	
    ImGui::CreateContext();
	ImGui_ImplVulkan_InitInfo ImGuiInitInfo{};
	ImGuiInitInfo.Instance = VulkanPlatform->Instance;
	ImGuiInitInfo.PhysicalDevice = VulkanPlatform->CurrentPhysicalDevice.PhysicalDevice;
	ImGuiInitInfo.Device = vkContext->Device;
	ImGuiInitInfo.QueueFamily = VulkanPlatform->CurrentPhysicalDevice.GraphicsQueueFamilyIndex;
	ImGuiInitInfo.Queue = vkContext->GraphicsQueue;
	ImGuiInitInfo.DescriptorPool = nullptr;
	ImGuiInitInfo.DescriptorPoolSize = 32;
	ImGuiInitInfo.RenderPass = vkRenderPass->RenderPass;
	ImGuiInitInfo.MinImageCount = 2;
	ImGuiInitInfo.ImageCount = vkWindow->CurrentSwapchain.SwapchainImageViews.size();
	ImGuiInitInfo.MSAASamples = vkRenderPass->msaaSamples;

	ImGui_ImplVulkan_Init(&ImGuiInitInfo);
	ImGui_ImplGlfw_InitForVulkan(vkWindow->pGLFWwindow, true);
}

void RHIVulkanImGUI::UpdateUI()
{
	static bool show_demo_window = true;
    static bool show_another_window = true;
    static glm::float4 clear_color;
    static ImGuiIO& io = ImGui::GetIO();
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
        ImGui::Text("io.WantCaptureMouse = %d", io.WantCaptureMouse);
        if (ImGui::Button("Close Me"))
            show_another_window = false;
        ImGui::End();
    }
    if (!io.WantCaptureMouse && (io.MouseDown[0] || io.MouseDown[1])) // User operate with actual scene
    {
        float DeltaX = io.MousePos.x - io.MousePosPrev.x;
        float DeltaY = io.MousePos.y - io.MousePosPrev.y;

        //viewMat = glm::rotate(glm::mat4(1.f), DeltaX * 0.01f, glm::vec3(0.0f, 1.0f, 0.0f)) * viewMat;
        //viewMat = glm::rotate(glm::mat4(1.f), DeltaY * 0.01f, glm::vec3(1.0f, 0.0f, 0.0f)) * viewMat;
		
		using float3 = glm::float3 ;
        float3 PlayerMove = { 0.f, 0.f, 0.f };
        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            PlayerMove += float3(0., 0., 1.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            PlayerMove += float3(0., 0., -1.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            PlayerMove += float3(1., 0., 0.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            PlayerMove += float3(-1., 0., 0.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_Q))
        {
            PlayerMove += float3(0., -1., 0.);
        }
        if (ImGui::IsKeyDown(ImGuiKey_E))
        {
            PlayerMove += float3(0., 1., 0.);
        }
        //viewMat = glm::translate(glm::mat4(1.0), PlayerMove * 0.001f) * viewMat;
    }
    // Rendering
    ImGui::Render();
}


void RHIVulkanImGUI::Cleanup()
{
	
}

void RHIVulkanImGUI::DispatchImGUI(RHIGraphicDispatcher* Dispatcher)
{
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
	auto* VkDispatcher = reinterpret_cast<RHIVulkanGraphicDispatcher*>(Dispatcher->GetImpl());
	ImGui_ImplVulkan_RenderDrawData(draw_data, VkDispatcher->CommandBuffer, nullptr);
}



void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger) {
	return RHIVulkanPlatformSupport::Get()->vkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkCmdPushDescriptorSetKHR(
    VkCommandBuffer                             commandBuffer,
    VkPipelineBindPoint                         pipelineBindPoint,
    VkPipelineLayout                            layout,
    uint32_t                                    set,
    uint32_t                                    descriptorWriteCount,
    const VkWriteDescriptorSet* pDescriptorWrites) {
	return RHIVulkanPlatformSupport::Get()->vkCmdPushDescriptorSetKHR(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites);
}