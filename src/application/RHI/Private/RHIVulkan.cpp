// RHIVulkan.cpp - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
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
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
		VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME
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

void RHIVulkanWindowManager::Initialize(RHIPlatformSupport* InPlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth)
{
	auto* VulkanPlatformSupport = static_cast<RHIVulkanPlatformSupport*>(InPlatformSupport->GetImpl());
	PlatformSupport = InPlatformSupport;
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

void RHIVulkanWindowManager::RecreateSwapchain(RHIContext* Context)
{
	CleanupSwapchain(Context);
	InitializeSwapchain(Context, PlatformSupport);
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
	VkImageUsageFlags VkImageUsage;
	VkImageAspectFlagBits VkImageAspectFlagBits;
	VkImageLayout VkLayout;
	Usage = InUsage;
	switch (Usage)
	{
	case IU_COLOR_RT:
		InnerFormat = VulkanWindowManager->CurrentSwapchain.SwapchainImageFormat;
		VkImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT;
		VkLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		CreateSampler(Sampler, VulkanContext->Device, RHIVulkanPlatformSupport::Get()->CurrentPhysicalDevice.PDProperties.limits.maxSamplerAnisotropy);
		bHasSampler = true;
		// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL
		break;
	case IU_COLOR_PRESENT_RT:
		InnerFormat = VulkanWindowManager->CurrentSwapchain.SwapchainImageFormat;
		VkImageUsage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT;
		VkLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		break;
	case IU_DEPTH_RT:
		InnerFormat = RHIVulkanPlatformSupport::Get()->GetDepthFormat();
		VkImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_DEPTH_BIT;
		VkLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		break;
	default:
		throw std::runtime_error("Invalid usage bit for init a rendertarget");
	}
	VkExtent3D vkExtent;
	vkExtent.height = RTExtent.Height;
	vkExtent.width = RTExtent.Width;
	vkExtent.depth = RTExtent.Depth;
	CreateImage(Image, VulkanContext->Device, vkExtent, 1, static_cast<VkSampleCountFlagBits>(MultiSamplesCount), InnerFormat, VK_IMAGE_TILING_OPTIMAL, VkImageUsage);
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(VulkanContext->Device, Image, &memRequirements);
	CreateDeviceMemory(DeviceMemory, VulkanContext->Device, memRequirements.size, RHIVulkanPlatformSupport::Get()->GetMemoryType(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	vkBindImageMemory(VulkanContext->Device, Image, DeviceMemory, 0);
	CreateImageView(ImageView, Image, VulkanContext->Device, InnerFormat, VkImageAspectFlagBits, 1);
	DescriptorInfo = { Sampler, ImageView, VkLayout };
}


void RHIVulkanImageResource::Initialize(RHIContext* Context, const char* ImageFileName, RHIFormat InFormat, uint32_t MipLevel)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	Usage = IU_GENERAL;
	InnerFormat = RHIVulkanPlatformSupport::GetVkFormat(InFormat);
	// Check if image format supports linear blitting
	VkFormatProperties formatProperties = RHIVulkanPlatformSupport::Get()->GetFormatProperties(InnerFormat);
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
		CreateImage(Image, VulkanContext->Device, ImageExtent, MipLevel, VK_SAMPLE_COUNT_1_BIT, InnerFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		vkGetImageMemoryRequirements(VulkanContext->Device, Image, &memRequirements);
		CreateDeviceMemory(DeviceMemory, VulkanContext->Device, memRequirements.size, RHIVulkanPlatformSupport::Get()->GetMemoryType(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
		vkBindImageMemory(VulkanContext->Device, Image, DeviceMemory, 0);
		CreateImageView(ImageView, Image, VulkanContext->Device, InnerFormat, VK_IMAGE_ASPECT_COLOR_BIT, MipLevel);
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
	DescriptorInfo = {Sampler, ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
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

void RHIVulkanPipeline::SetUniformBinding(RHIUniform* Uniform, uint32_t Binding)
{
	auto* VulkanUniform = static_cast<RHIVulkanUniform*>(Uniform->GetImpl());
	VkWriteDescriptorSet WriteDescSet{};
	WriteDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	WriteDescSet.dstSet = nullptr;
	WriteDescSet.dstBinding = Binding;
	WriteDescSet.dstArrayElement = 0;
	WriteDescSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	WriteDescSet.descriptorCount = 1;
	WriteDescSet.pBufferInfo = &VulkanUniform->DescriptorBufferInfo;

	bool HasDescSet = false;
	for(auto& DescSet : WriteDescriptorSets)
	{
		if(DescSet.dstBinding==Binding)
		{
			DescSet = WriteDescSet;
			HasDescSet = true;
		}
	}
	if(!HasDescSet)
	{
		WriteDescriptorSets.push_back(WriteDescSet);
		UniformBufferDescriptorCount++;
	}

	VkDescriptorSetLayoutBinding uboLayoutBinding;
    uboLayoutBinding.binding = Binding;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
	bool HasLayoutBinding = false;
	for(auto& SetLayoutBinding : DescSetLayoutBindings)
	{
		if(SetLayoutBinding.binding==Binding)
		{
			SetLayoutBinding = uboLayoutBinding;
			HasLayoutBinding = true;
		}
	}
	if(!HasLayoutBinding) {
		DescSetLayoutBindings.push_back(uboLayoutBinding);
	}
}

void RHIVulkanPipeline::SetImageSamplerBinding(RHIImageResource* ImageResource, uint32_t Binding)
{
	auto* VulkanImageResource = static_cast<RHIVulkanImageResource*>(ImageResource->GetImpl());
	VkWriteDescriptorSet WriteDescSet{};
	WriteDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	WriteDescSet.dstSet = nullptr;
	WriteDescSet.dstBinding = Binding;
	WriteDescSet.dstArrayElement = 0;
	WriteDescSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	WriteDescSet.descriptorCount = 1;
	WriteDescSet.pImageInfo = &VulkanImageResource->DescriptorInfo;
	bool HasDescSet = false;
	for(auto& DescSet : WriteDescriptorSets)
	{
		if(DescSet.dstBinding==Binding)
		{
			DescSet = WriteDescSet;
			HasDescSet = true;
		}
	}
	if(!HasDescSet)
	{
		WriteDescriptorSets.push_back(WriteDescSet);
		CombinedImageSamplerDescriptorCount++;
	}

    VkDescriptorSetLayoutBinding samplerLayoutBinding;
    samplerLayoutBinding.binding = Binding;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bool HasLayoutBinding = false;
	for(auto& SetLayoutBinding : DescSetLayoutBindings)
	{
		if(SetLayoutBinding.binding==Binding)
		{
			SetLayoutBinding = samplerLayoutBinding;
			HasLayoutBinding = true;
		}
	}
	if(!HasLayoutBinding) {
		DescSetLayoutBindings.push_back(samplerLayoutBinding);
	}
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
	CreateGraphicsPipeline(Pipeline, PipelineLayout, VK_SAMPLE_COUNT_1_BIT, VulkanContext->Device, VertShaderBytecode, "main", FragShaderBytecode, "main",
		BindingDescriptions, AttributeDescriptions, DescriptorSetLayout, VulkanRenderPassResource->RenderPass);
}


void RHIVulkanPipeline::Initialize(RHIContext* Context, RHIPresentPass* PresentPass)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanPresentPassResource = static_cast<RHIVulkanPresentPass*>(PresentPass->GetImpl());
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
	CreateGraphicsPipeline(Pipeline, PipelineLayout, VulkanPresentPassResource->msaaSamples, VulkanContext->Device, VertShaderBytecode, "main", FragShaderBytecode, "main",
		BindingDescriptions, AttributeDescriptions, DescriptorSetLayout, VulkanPresentPassResource->RenderPass);
}


void RHIVulkanPipeline::Cleanup(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	vkDestroyPipeline(VulkanContext->Device, Pipeline, nullptr);
	vkDestroyPipelineLayout(VulkanContext->Device, PipelineLayout, nullptr);
	vkDestroyDescriptorPool(VulkanContext->Device, DescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(VulkanContext->Device, DescriptorSetLayout, nullptr);
}

void RHIVulkanRenderPass::Initialize(RHIContext* Context, uint32_t Width, uint32_t Height)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	Extent.height = Height;
	Extent.width = Width;
	std::vector<VkImageView> ImageViews;
	for(size_t i = 0; i < ColorRenderTargets.size(); i++)
	{
		auto* VkImageResource = ColorRenderTargets[i];
		VkAttachmentDescription AttachmentDesc{};
		AttachmentDesc.format = VkImageResource->InnerFormat;
		AttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		AttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		AttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		AttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		AttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		AttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		AttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		Attachments.emplace_back(
			AttachmentDesc
		);
		ImageViews.push_back(VkImageResource->ImageView);
	}
	if(DepthRenderTargets!=nullptr)
	{
		VkAttachmentDescription AttachmentDesc{};
		AttachmentDesc.format = DepthRenderTargets->InnerFormat;
		AttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		AttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		AttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		AttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		AttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		AttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		AttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		Attachments.emplace_back(
			AttachmentDesc
		);
		DepthAttachmentIndex = ColorRenderTargets.size();
		ImageViews.push_back(DepthRenderTargets->ImageView);
	}
	CreateRenderPassSingleSubpass(RenderPass, VulkanContext->Device, Attachments, DepthAttachmentIndex);
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = RenderPass;
	framebufferInfo.attachmentCount = static_cast<uint32_t>(ImageViews.size());
	framebufferInfo.pAttachments = ImageViews.data();
	framebufferInfo.width = Width;
	framebufferInfo.height = Height;
	framebufferInfo.layers = 1;

	if (vkCreateFramebuffer(VulkanContext->Device, &framebufferInfo, nullptr, &FrameBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create framebuffer!");
	}
}

void RHIVulkanRenderPass::Cleanup(RHIContext* Context)
{

}

void RHIVulkanRenderPass::AddColorRenderTarget(RHIImageResource* ColorRT)
{
	ColorRenderTargets.push_back(static_cast<RHIVulkanImageResource*>(ColorRT->GetImpl()));
}

void RHIVulkanRenderPass::SetDepthRenderTarget(RHIImageResource* DepthRT)
{
	DepthRenderTargets = static_cast<RHIVulkanImageResource*>(DepthRT->GetImpl());
}


// RHIVulkanPresentPass implementation
RHIVulkanPresentPass::RHIVulkanPresentPass()
{
    // Initialize any necessary Vulkan-specific resources here
}

RHIVulkanPresentPass::~RHIVulkanPresentPass()
{
    // Cleanup any Vulkan-specific resources here
}

void RHIVulkanPresentPass::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples, RHIImageResource* InColorRT, RHIImageResource* InDepthRT)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	ColorRT = static_cast<RHIVulkanImageResource*>(InColorRT->GetImpl());
	DepthRT = static_cast<RHIVulkanImageResource*>(InDepthRT->GetImpl());
	msaaSamples = VK_SAMPLE_COUNT_4_BIT;
	CreatePresentableRenderPass(RenderPass, VulkanContext->Device, RHIVulkanPlatformSupport::Get()->GetDepthFormat(), VulkanWindowManager->CurrentSwapchain.SwapchainImageFormat, msaaSamples);
	CreateSwapchainFramebuffer(Context, WindowManager);
}

void RHIVulkanPresentPass::CreateSwapchainFramebuffer(RHIContext* Context, RHIWindowManager* WindowManager)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	SwapchainFramebuffers.resize(VulkanWindowManager->CurrentSwapchain.SwapchainImageViews.size());
	for (size_t i = 0; i < VulkanWindowManager->CurrentSwapchain.SwapchainImageViews.size(); i++) {
		std::array<VkImageView, 3> attachments = {
			ColorRT->ImageView,
			DepthRT->ImageView,
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

void RHIVulkanPresentPass::CleanupSwapchainFramebuffer(RHIContext* Context)
{
    auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	for (auto framebuffer : SwapchainFramebuffers) {
		vkDestroyFramebuffer(VulkanContext->Device, framebuffer, nullptr);
	}
}

void RHIVulkanPresentPass::Cleanup(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	CleanupSwapchainFramebuffer(Context);
	vkDestroyRenderPass(VulkanContext->Device, RenderPass, nullptr);
}

void RHIVulkanPresentPass::OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager)
{
	WindowManager->RecreateSwapchain(Context);
	CleanupSwapchainFramebuffer(Context);
	ColorRT->Cleanup(Context);
	DepthRT->Cleanup(Context);
	ImageExtent3D RenderTargetExtent;
	RenderTargetExtent.Height = WindowManager->GetWindowHeight();
	RenderTargetExtent.Width = WindowManager->GetWindowWidth();
	RenderTargetExtent.Depth = 1;
	ColorRT->InitializeRenderTarget(Context, WindowManager, RenderTargetExtent, IU_COLOR_RT, msaaSamples);
	DepthRT->InitializeRenderTarget(Context, WindowManager, RenderTargetExtent, IU_DEPTH_RT, msaaSamples);
	CreateSwapchainFramebuffer(Context, WindowManager);
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
	if (VulkanPipeline->WriteDescriptorSets.size()>0)
	{
		vkCmdPushDescriptorSetKHR(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipeline->PipelineLayout,
			0, VulkanPipeline->WriteDescriptorSets.size(), VulkanPipeline->WriteDescriptorSets.data());
	}
	
	//if (VulkanPipeline->DescriptorSet)
	//{
	//	
	//	//vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipeline->PipelineLayout, 0, 1,
	//	//	&VulkanPipeline->DescriptorSet, 0, nullptr);
	//}
	
	vkCmdDrawIndexed(CommandBuffer, IndexCount, InstanceCount, IndexOffset, 0, 0);
}

void RHIVulkanGraphicDispatcher::BeginRenderPass(RHIContext* Context, RHIRenderPass* RenderPass)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanRenderPass = static_cast<RHIVulkanRenderPass*>(RenderPass->GetImpl());

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = VulkanRenderPass->RenderPass;
	renderPassInfo.framebuffer = VulkanRenderPass->FrameBuffer;
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = VulkanRenderPass->Extent;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { {1.0f, 0.0f, 1.0f, 1.0f} };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	
}

void RHIVulkanGraphicDispatcher::EndRenderPass(RHIRenderPass* RenderPass)
{
	auto* VulkanRenderPass = static_cast<RHIVulkanRenderPass*>(RenderPass->GetImpl());
	vkCmdEndRenderPass(CommandBuffer);
}

void RHIVulkanGraphicDispatcher::BeginPresentPass(RHIContext* Context, RHIWindowManager* WindowManager, RHIPresentPass* PresentPassResource)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	auto* VulkanPresentPassResource = static_cast<RHIVulkanPresentPass*>(PresentPassResource->GetImpl());

	glfwPollEvents();
	WaitForGPUIdle(Context);
	if (bWindowResizeLastframe)
	{
		PresentPassResource->OnWindowResize(Context, WindowManager);
		bWindowResizeLastframe = false;
	}
	VkResult result = vkAcquireNextImageKHR(VulkanContext->Device, VulkanWindowManager->CurrentSwapchain.Swapchain, UINT64_MAX, ImageAvailableSemaphore, VK_NULL_HANDLE, &CurrentImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		
		PresentPassResource->OnWindowResize(Context, WindowManager);
		bWindowResizeLastframe = false;
		VkResult result = vkAcquireNextImageKHR(VulkanContext->Device, VulkanWindowManager->CurrentSwapchain.Swapchain, UINT64_MAX, ImageAvailableSemaphore, VK_NULL_HANDLE, &CurrentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain after retry!");
		}
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	vkResetFences(VulkanContext->Device, 1, &InFlightFence);

	//vkResetCommandBuffer(CommandBuffer, /*VkCommandBufferResetFlagBits*/ 0);

	//VkCommandBufferBeginInfo beginInfo{};
	//beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	//if (vkBeginCommandBuffer(CommandBuffer, &beginInfo) != VK_SUCCESS) {
	//	throw std::runtime_error("failed to begin recording command buffer!");
	//}

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = VulkanPresentPassResource->RenderPass;
	renderPassInfo.framebuffer = VulkanPresentPassResource->SwapchainFramebuffers[CurrentImageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = VulkanWindowManager->CurrentSwapchain.SwapchainExtent;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = { {1.0f, 0.0f, 0.0f, 1.0f} };
	clearValues[1].depthStencil = { 1000000.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void RHIVulkanGraphicDispatcher::WaitForGPUIdle(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	vkWaitForFences(VulkanContext->Device, 1, &InFlightFence, VK_TRUE, UINT64_MAX);
}


void RHIVulkanGraphicDispatcher::EndPresentPassAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindowManager = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	vkCmdEndRenderPass(CommandBuffer);
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
	presentInfo.pImageIndices = &CurrentImageIndex;

	auto result = vkQueuePresentKHR(VulkanContext->PresentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		bWindowResizeLastframe = true;
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}
}

void RHIVulkanGraphicDispatcher::BeginFrame()
{
	vkResetCommandBuffer(CommandBuffer, /*VkCommandBufferResetFlagBits*/ 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(CommandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}
}


void RHIVulkanImGUI::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIPresentPass* PresentPass)
{
	auto VulkanPlatform = static_cast<RHIVulkanPlatformSupport*>(RHIPlatformSupport::Get()->GetImpl());
	auto vkWindow = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	auto vkContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto vkPresentPass = static_cast<RHIVulkanPresentPass*>(PresentPass->GetImpl());
	
    ImGlobals.Context = ImGui::CreateContext();
	ImGui::GetAllocatorFunctions(&ImGlobals.MemAllocFunc, &ImGlobals.MemFreeFunc, &ImGlobals.UserData);
	ImGui_ImplVulkan_InitInfo ImGuiInitInfo{};
	ImGuiInitInfo.Instance = VulkanPlatform->Instance;
	ImGuiInitInfo.PhysicalDevice = VulkanPlatform->CurrentPhysicalDevice.PhysicalDevice;
	ImGuiInitInfo.Device = vkContext->Device;
	ImGuiInitInfo.QueueFamily = VulkanPlatform->CurrentPhysicalDevice.GraphicsQueueFamilyIndex;
	ImGuiInitInfo.Queue = vkContext->GraphicsQueue;
	ImGuiInitInfo.DescriptorPool = nullptr;
	ImGuiInitInfo.DescriptorPoolSize = 32;
	ImGuiInitInfo.RenderPass = vkPresentPass->RenderPass;
	ImGuiInitInfo.MinImageCount = 2;
	ImGuiInitInfo.ImageCount = vkWindow->CurrentSwapchain.SwapchainImageViews.size();
	ImGuiInitInfo.MSAASamples = vkPresentPass->msaaSamples;

	ImGui_ImplVulkan_Init(&ImGuiInitInfo);
	ImGui_ImplGlfw_InitForVulkan(vkWindow->pGLFWwindow, true);
}

void RHIVulkanImGUI::UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context))
{
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
	pFuncDrawUI(&ImGlobals);
    // Rendering
    ImGui::Render();
}


void RHIVulkanImGUI::Cleanup()
{
	
}

void RHIVulkanImGUI::DispatchImGUI(RHIGraphicsDispatcher* Dispatcher)
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
