#define RHI_IMPLEMENT
#include "RHI.h"
#include "RHIVulkan.h"

#include <stdexcept>
#include <vector>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <array>
#include <cassert>
#include <optional>
#include <set>
#include <stb_image.h>
#include <unordered_map>
#include "GLFW/glfw3.h"

RHIVulkanContext::RHIVulkanContext()
{
}

void RHIVulkanContext::Initialize(VulkanContextInfo CreateInfo)
{
	CreateGLFWWindow(pGLFWwindow, CreateInfo.WindowWidth, CreateInfo.WindowHeight, this, OnWindowResize);
	CreateVkInstance(Instance, DebugMessenger, Extensions);
	CreateVkSurface(Instance, pGLFWwindow, Surface);
	PhysicalDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	RetrieveAvailablePhysicalDevices(AvailablePhysicalDevices, Instance);

	for (auto& CurrentPhysicalDevice : AvailablePhysicalDevices)
	{
		SurfaceFormats.clear();
		PresentModes.clear();
		vkGetPhysicalDeviceFeatures(CurrentPhysicalDevice, &supportedFeatures);
		if (PhysicalDeviceSupportExtensions(CurrentPhysicalDevice, PhysicalDeviceExtensions)
			&& PhysicalDeviceSupportSurface(SurfaceCapabilities, SurfaceFormats, PresentModes, CurrentPhysicalDevice, Surface)
			&& PhysicalDevideSupportQueueFamilies(GraphicsQueueFamilyIndex, PresentQueueFamilyIndex, CurrentPhysicalDevice, Surface)
			&& supportedFeatures.samplerAnisotropy)
		{
			PhysicalDevice = CurrentPhysicalDevice;
			break;
		}
	}

	if (PhysicalDevice == VK_NULL_HANDLE)
	{
		bIsValid = false;
		throw std::runtime_error("failed to find a suitable GPU!");
	}

	CreateVkDevice(Device, GraphicsQueue, PresentQueue, PhysicalDevice, 
		GraphicsQueueFamilyIndex, PresentQueueFamilyIndex, PhysicalDeviceExtensions);
	CreateCommandPool(CommandPool, Device, GraphicsQueueFamilyIndex);
	vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &PDMemoryProperties);
	DepthFormat = findDepthFormat(PhysicalDevice);

	InitializeSwapchain();
}

void RHIVulkanContext::CleanupSwapchain()
{
	for (auto imageView : SwapchainImageViews) {
		vkDestroyImageView(Device, imageView, nullptr);
	}
	vkDestroySwapchainKHR(Device, Swapchain, nullptr);
}

void RHIVulkanContext::Cleanup()
{
	CleanupSwapchain();
	vkDestroyCommandPool(Device, CommandPool, nullptr);
	vkDestroyDevice(Device, nullptr);

	//if (enableValidationLayers) {
	//    DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	//}

	vkDestroySurfaceKHR(Instance, Surface, nullptr);
	vkDestroyInstance(Instance, nullptr);

	glfwDestroyWindow(pGLFWwindow);

	glfwTerminate();
}

void RHIVulkanContext::InitializeSwapchain()
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &SurfaceCapabilities);
	glfwGetFramebufferSize(pGLFWwindow, reinterpret_cast<int*>(&SwapchainExtent.width), reinterpret_cast<int*>(&SwapchainExtent.height));
	while (SwapchainExtent.width == 0 || SwapchainExtent.height == 0) {
		glfwGetFramebufferSize(pGLFWwindow, reinterpret_cast<int*>(&SwapchainExtent.width), reinterpret_cast<int*>(&SwapchainExtent.height));
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(Device);
	CreateSwapchain(Swapchain, SwapchainImages, SwapchainImageViews, SwapchainImageFormat, SwapchainExtent,
		Device, Surface, SurfaceCapabilities, SurfaceFormats, PresentModes, GraphicsQueueFamilyIndex, PresentQueueFamilyIndex);
}

void RHIVulkanContext::OnWindowResize(GLFWwindow* window, int width, int height)
{
	auto self = reinterpret_cast<RHIVulkanContext*>(glfwGetWindowUserPointer(window));
	std::cout<<"Windows Resize!\n";
	// todo resize
}

void RHIVulkanContext::WaitDeviceIdle()
{
	vkDeviceWaitIdle(Device);
}

bool RHIVulkanContext::WindowActive()
{
	return !glfwWindowShouldClose(pGLFWwindow);
}


void RHIVulkanImageResource::Initialize(RHIVulkanContext* Context, VkExtent3D ImageExtent, uint32_t MipLevel, VkSampleCountFlagBits numSamples,
	VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlagBits ImageAspectFlagBits, VkMemoryPropertyFlags properties)
{
	CreateImageAndDeviceMemory(Image, DeviceMemory, Context->Device, ImageExtent, MipLevel, numSamples, format, tiling, usage, properties, Context->PDMemoryProperties);
	CreateImageView(ImageView, Image, Context->Device, format, ImageAspectFlagBits, MipLevel);
}

void RHIVulkanImageResource::Initialize(RHIVulkanContext* Context, const char* ImageFileName, VkSampleCountFlagBits numSamples,
	VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkImageAspectFlagBits ImageAspectFlagBits, VkMemoryPropertyFlags properties)
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(ImageFileName, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	assert(texHeight > 0 && texWidth > 0);
	VkDeviceSize imageSize = texWidth * texHeight * 4;
	uint32_t MipLevel = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CreateBufferAndDeviceMemory(stagingBuffer, stagingBufferMemory, Context->Device, imageSize,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, Context->PDMemoryProperties);

	void* data;
	vkMapMemory(Context->Device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(Context->Device, stagingBufferMemory);

	stbi_image_free(pixels);
	VkExtent3D ImageExtent;
	ImageExtent.height = texHeight;
	ImageExtent.width = texWidth;
	ImageExtent.depth = 1;
	CreateImageAndDeviceMemory(Image, DeviceMemory, Context->Device, ImageExtent, MipLevel, numSamples, format, tiling, usage, properties, Context->PDMemoryProperties);

	{
		VkCommandBuffer commandBuffer;
		CreateCommandBuffer(commandBuffer, Context->Device, Context->CommandPool);
		BeginCommandBufferOneTimeSubmit(commandBuffer, Context->CommandPool, Context->Device);
		TransitionImageLayout(Image, commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, MipLevel);
		EndCommandBufferOneTimeSubmit(commandBuffer, Context->CommandPool, Context->GraphicsQueue, Context->Device);
	}

	{
		VkCommandBuffer commandBuffer;
		CreateCommandBuffer(commandBuffer, Context->Device, Context->CommandPool);
		BeginCommandBufferOneTimeSubmit(commandBuffer, Context->CommandPool, Context->Device);
		CopyBufferToImage(stagingBuffer, Image, commandBuffer, ImageExtent.width, ImageExtent.height);
		EndCommandBufferOneTimeSubmit(commandBuffer, Context->CommandPool, Context->GraphicsQueue, Context->Device);
	}
	//transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps

	vkDestroyBuffer(Context->Device, stagingBuffer, nullptr);
	vkFreeMemory(Context->Device, stagingBufferMemory, nullptr);

	// generate mipmaps

	{
		// Check if image format supports linear blitting
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(Context->PhysicalDevice, format, &formatProperties);

		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
			throw std::runtime_error("texture image format does not support linear blitting!");
		}

		VkCommandBuffer commandBuffer;
		CreateCommandBuffer(commandBuffer, Context->Device, Context->CommandPool);
		BeginCommandBufferOneTimeSubmit(commandBuffer, Context->CommandPool, Context->Device);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = Image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for (uint32_t i = 1; i < MipLevel; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
				Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = MipLevel - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		EndCommandBufferOneTimeSubmit(commandBuffer, Context->CommandPool, Context->GraphicsQueue, Context->Device);
	}

	CreateImageView(ImageView, Image, Context->Device, format, ImageAspectFlagBits, MipLevel);
	CreateSampler(Sampler, Context->Device, Context->PhysicalDevice);
	bHasSampler = true;
}

void RHIVulkanImageResource::Cleanup(RHIVulkanContext* Context)
{
	vkDestroyImage(Context->Device, Image, nullptr);
	vkDestroyImageView(Context->Device, ImageView, nullptr);
	vkFreeMemory(Context->Device, DeviceMemory, nullptr);
	if (bHasSampler)
	{
		vkDestroySampler(Context->Device, Sampler, nullptr);
	}
}

void RHIVulkanBufferResource::Initialize(RHIVulkanContext* Context, uint32_t Stride, uint32_t ElementCounts, VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties)
{
	CreateBufferAndDeviceMemory(Buffer, DeviceMemory, Context->Device, Stride * ElementCounts, usage, properties, Context->PDMemoryProperties);
}

void RHIVulkanBufferResource::CopyToBuffer(RHIVulkanContext* Context, void* data, uint32_t TotalBytes)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	CreateBufferAndDeviceMemory(stagingBuffer, stagingBufferMemory, Context->Device, TotalBytes,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, Context->PDMemoryProperties);

	void* pMappedBuffer;
	vkMapMemory(Context->Device, stagingBufferMemory, 0, TotalBytes, 0, &pMappedBuffer);
	memcpy(pMappedBuffer, data, TotalBytes);
	vkUnmapMemory(Context->Device, stagingBufferMemory);
	VkCommandBuffer CommandBuffer;
	CreateCommandBuffer(CommandBuffer, Context->Device, Context->CommandPool);
	BeginCommandBufferOneTimeSubmit(CommandBuffer, Context->CommandPool, Context->Device);
	CopyBuffer(stagingBuffer, Buffer, TotalBytes, CommandBuffer);
	EndCommandBufferOneTimeSubmit(CommandBuffer, Context->CommandPool, Context->GraphicsQueue, Context->Device);

}

void RHIVulkanBufferResource::Cleanup(RHIVulkanContext* Context)
{
	vkDestroyBuffer(Context->Device, Buffer, nullptr);
	vkFreeMemory(Context->Device, DeviceMemory, nullptr);
}

void RHIVulkanUniform::Initialize(RHIVulkanContext* Context, uint32_t UniformStructSize)
{
	CreateBufferAndDeviceMemory(Buffer, DeviceMemory, Context->Device, UniformStructSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, Context->PDMemoryProperties);
	vkMapMemory(Context->Device, DeviceMemory, 0, UniformStructSize, 0, &MappedMemory);
	DescriptorBufferInfo = {};
	DescriptorBufferInfo.buffer = Buffer;
	DescriptorBufferInfo.offset = 0;
	DescriptorBufferInfo.range = UniformStructSize;
}

void RHIVulkanUniform::CopyToBuffer(RHIVulkanContext* Context, void* data, uint32_t TotalBytes)
{
	memcpy(MappedMemory, data, TotalBytes);
}

void RHIVulkanUniform::Cleanup(RHIVulkanContext* Context)
{
	vkUnmapMemory(Context->Device, DeviceMemory);
	vkDestroyBuffer(Context->Device, Buffer, nullptr);
	vkFreeMemory(Context->Device, DeviceMemory, nullptr);
}

void RHIVulkanPipeline::AddLayout(uint32_t BindingIndex, uint32_t Location, VkFormat Format, uint32_t Offset)
{
	AttributeDescriptions.push_back({ Location, BindingIndex, Format, Offset });
}

void RHIVulkanPipeline::AddBinding(uint32_t BindingIndex, uint32_t Stride)
{
	BindingDescriptions.push_back({ BindingIndex, Stride, VK_VERTEX_INPUT_RATE_VERTEX });
}

void RHIVulkanPipeline::AddUniformBuffer(const VkDescriptorBufferInfo& UniformDescBufferInfo)
{
	VkWriteDescriptorSet WriteDescSet{};
	WriteDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	WriteDescSet.dstSet = nullptr;
	WriteDescSet.dstBinding = 0;
	WriteDescSet.dstArrayElement = 0;
	WriteDescSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	WriteDescSet.descriptorCount = 1;
	WriteDescSet.pBufferInfo = &UniformDescBufferInfo;
	WriteDescriptorSets.push_back(WriteDescSet);
	UniformBufferDescriptorCount++;
}

void RHIVulkanPipeline::AddImageSampler(const VkDescriptorImageInfo& DescImageInfo)
{
	VkWriteDescriptorSet WriteDescSet{};
	WriteDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	WriteDescSet.dstSet = nullptr;
	WriteDescSet.dstBinding = 1;
	WriteDescSet.dstArrayElement = 0;
	WriteDescSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	WriteDescSet.descriptorCount = 1;
	WriteDescSet.pImageInfo = &DescImageInfo;
	WriteDescriptorSets.push_back(WriteDescSet);
	CombinedImageSamplerDescriptorCount++;
}


void RHIVulkanPipeline::Initialize(RHIVulkanContext* Context, RHIVulkanRenderPass* RenderPassResource, const std::vector<char>& VertShader, const std::vector<char>& FragShader)
{
	VertShaderBytecode = VertShader;
	FragShaderBytecode = FragShader;

	if (UniformBufferDescriptorCount>0 || CombinedImageSamplerDescriptorCount > 0)
	{
		CreateDescriptorSetLayout(DescriptorSetLayout, Context->Device);
		CreateDescriptorPool(DescriptorPool, Context->Device, UniformBufferDescriptorCount, CombinedImageSamplerDescriptorCount);
		CreateDescriptorSet(DescriptorSet, WriteDescriptorSets, Context->Device, DescriptorPool, DescriptorSetLayout);
	}
	else
	{
		DescriptorSetLayout = nullptr;
		DescriptorPool = nullptr;
		DescriptorSet = nullptr;
	}
	CreateGraphicsPipeline(Pipeline, PipelineLayout, RenderPassResource->msaaSamples, Context->Device, VertShaderBytecode, "main", FragShaderBytecode, "main",
		BindingDescriptions, AttributeDescriptions, DescriptorSetLayout, RenderPassResource->RenderPass);
}


void RHIVulkanPipeline::Cleanup(RHIVulkanContext* Context)
{
	vkDestroyPipeline(Context->Device, Pipeline, nullptr);
	vkDestroyPipelineLayout(Context->Device, PipelineLayout, nullptr);
	vkDestroyDescriptorPool(Context->Device, DescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(Context->Device, DescriptorSetLayout, nullptr);
}

void RHIVulkanRenderPass::Initialize(RHIVulkanContext* Context)
{
	msaaSamples = GetMaxUsableSampleCount(Context->PhysicalDevice);
	CreateRenderPass(RenderPass, Context->Device, Context->PhysicalDevice, Context->SwapchainImageFormat, msaaSamples);
	VkExtent3D RenderTargetExtent;
	RenderTargetExtent.height = Context->SwapchainExtent.height;
	RenderTargetExtent.width = Context->SwapchainExtent.width;
	RenderTargetExtent.depth = 1;
	CreateColorRenderTarget(Context, RenderTargetExtent);
	CreateDepthRenderTarget(Context, RenderTargetExtent);
	CreateSwapchainFramebuffer(Context);
}

void RHIVulkanRenderPass::CreateColorRenderTarget(RHIVulkanContext* Context, VkExtent3D RTExtent)
{
	ColorRenderTargetResource.Initialize(Context, RTExtent, 1, msaaSamples, Context->SwapchainImageFormat, 
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void RHIVulkanRenderPass::CreateDepthRenderTarget(RHIVulkanContext* Context, VkExtent3D RTExtent)
{
	DepthRenderTargetResource.Initialize(Context, RTExtent, 1, msaaSamples, Context->DepthFormat,
		VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

void RHIVulkanRenderPass::CreateSwapchainFramebuffer(RHIVulkanContext* Context)
{
	SwapchainFramebuffers.resize(Context->SwapchainImageViews.size());
	for (size_t i = 0; i < Context->SwapchainImageViews.size(); i++) {
		std::array<VkImageView, 3> attachments = {
			ColorRenderTargetResource.ImageView,
			DepthRenderTargetResource.ImageView,
			Context->SwapchainImageViews[i]
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = RenderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = Context->SwapchainExtent.width;
		framebufferInfo.height = Context->SwapchainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(Context->Device, &framebufferInfo, nullptr, &SwapchainFramebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}
void RHIVulkanRenderPass::CleanupSwapchainFramebuffer(RHIVulkanContext* Context)
{
	for (auto framebuffer : SwapchainFramebuffers) {
		vkDestroyFramebuffer(Context->Device, framebuffer, nullptr);
	}
}

void RHIVulkanRenderPass::Cleanup(RHIVulkanContext* Context)
{
	CleanupSwapchainFramebuffer(Context);
	// clean up
	ColorRenderTargetResource.Cleanup(Context);
	DepthRenderTargetResource.Cleanup(Context);
	vkDestroyRenderPass(Context->Device, RenderPass, nullptr);
}


void RHIVulkanGraphicDispatcher::Initialize(RHIVulkanContext* Context)
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	CreateCommandBuffer(CommandBuffer, Context->Device, Context->CommandPool);
	if (vkCreateSemaphore(Context->Device, &semaphoreInfo, nullptr, &ImageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(Context->Device, &semaphoreInfo, nullptr, &RenderFinishSemaphore) != VK_SUCCESS ||
		vkCreateFence(Context->Device, &fenceInfo, nullptr, &InFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("failed to create synchronization objects for a frame!");
	}
}

void RHIVulkanGraphicDispatcher::Cleanup(RHIVulkanContext* Context)
{
	for (int i = 0; i < Context->SwapchainImageViews.size(); i++)
	{
		vkDestroySemaphore(Context->Device, ImageAvailableSemaphore, nullptr);
		vkDestroySemaphore(Context->Device, RenderFinishSemaphore, nullptr);
		vkDestroyFence(Context->Device, InFlightFence, nullptr);
	}
}

void RHIVulkanGraphicDispatcher::BindVertexBuffer(RHIVulkanBufferResource* BufferResource, VkDeviceSize Offset, uint32_t BindingIndex)
{
	BindingInfo Info;
	Info.BindingIndex = BindingIndex;
	Info.BufferResource = BufferResource;
	Info.Offset = Offset;
	BindingInfos.push_back(Info);
}

void RHIVulkanGraphicDispatcher::BindIndexBuffer(RHIVulkanBufferResource* BufferResource, VkDeviceSize Offset)
{
	IndexBindingInfo.BufferResource = BufferResource;
	IndexBindingInfo.Offset = Offset;
}

void RHIVulkanGraphicDispatcher::Dispatch(RHIVulkanContext* Context, RHIVulkanPipeline* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount)
{
	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline->Pipeline);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)Context->SwapchainExtent.width;
	viewport.height = (float)Context->SwapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(CommandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = Context->SwapchainExtent;
	vkCmdSetScissor(CommandBuffer, 0, 1, &scissor);

	for (auto& Info : BindingInfos)
	{
		vkCmdBindVertexBuffers(CommandBuffer, Info.BindingIndex, 1, &Info.BufferResource->Buffer, &Info.Offset);
	}
	vkCmdBindIndexBuffer(CommandBuffer, IndexBindingInfo.BufferResource->Buffer, IndexBindingInfo.Offset, VK_INDEX_TYPE_UINT32);
	if (Pipeline->DescriptorSet)
	{
		vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline->PipelineLayout, 0, 1,
			&Pipeline->DescriptorSet, 0, nullptr);
	}
	
	vkCmdDrawIndexed(CommandBuffer, IndexCount, InstanceCount, IndexOffset, 0, 0);
}

void RHIVulkanGraphicDispatcher::DispatchImGUI(ImDrawData* draw_data, 
	void (*ImGui_ImplVulkan_RenderDrawData)(ImDrawData* draw_data, VkCommandBuffer command_buffer, VkPipeline pipeline))
{
	ImGui_ImplVulkan_RenderDrawData(draw_data, CommandBuffer, nullptr);
}

void RHIVulkanGraphicDispatcher::PrepareRenderPass(RHIVulkanContext* Context, uint32_t& OutImageIndex, RHIVulkanRenderPass* RenderPass)
{
	glfwPollEvents();
	vkWaitForFences(Context->Device, 1, &InFlightFence, VK_TRUE, UINT64_MAX);
	VkResult result = vkAcquireNextImageKHR(Context->Device, Context->Swapchain, UINT64_MAX, ImageAvailableSemaphore, VK_NULL_HANDLE, &OutImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		Context->CleanupSwapchain();
		Context->InitializeSwapchain();
		RenderPass->CleanupSwapchainFramebuffer(Context);
		RenderPass->ColorRenderTargetResource.Cleanup(Context);
		RenderPass->DepthRenderTargetResource.Cleanup(Context);
		VkExtent3D RenderTargetExtent;
		RenderTargetExtent.height = Context->SwapchainExtent.height;
		RenderTargetExtent.width = Context->SwapchainExtent.width;
		RenderTargetExtent.depth = 1;
		RenderPass->CreateColorRenderTarget(Context, RenderTargetExtent);
		RenderPass->CreateDepthRenderTarget(Context, RenderTargetExtent);
		RenderPass->CreateSwapchainFramebuffer(Context);
		VkResult result = vkAcquireNextImageKHR(Context->Device, Context->Swapchain, UINT64_MAX, ImageAvailableSemaphore, VK_NULL_HANDLE, &OutImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain after retry!");
		}
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	vkResetFences(Context->Device, 1, &InFlightFence);

	vkResetCommandBuffer(CommandBuffer, /*VkCommandBufferResetFlagBits*/ 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(CommandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}
}


void RHIVulkanGraphicDispatcher::BeginRenderPass(RHIVulkanContext* Context, RHIVulkanRenderPass* RenderPassResource, uint32_t ImageIndex)
{
	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = RenderPassResource->RenderPass;
	renderPassInfo.framebuffer = RenderPassResource->SwapchainFramebuffers[ImageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = Context->SwapchainExtent;

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

void RHIVulkanGraphicDispatcher::Submit(RHIVulkanContext* Context, uint32_t ImageIndex)
{
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

	if (vkQueueSubmit(Context->GraphicsQueue, 1, &submitInfo, InFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &RenderFinishSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &Context->Swapchain;
	presentInfo.pImageIndices = &ImageIndex;

	auto result = vkQueuePresentKHR(Context->PresentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		//framebufferResized = false;
		//recreateSwapChain();
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}

}


void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}
