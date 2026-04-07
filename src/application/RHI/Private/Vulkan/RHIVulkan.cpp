// RHIVulkan.cpp - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
#define RHI_IMPLEMENT
#include "RHI.h"
#include "RHIVulkan.h"

#include <stdexcept>
#include <vector>
#include <cstdint>
#include <cassert>
#include <iostream>
#include <array>

#include "RHIVulkanImpl.h"
#include "CoreLog.inl"
#include "GLFW/glfw3.h"

RHIVulkanPlatformSupport::RHIVulkanPlatformSupport() {}

void RHIVulkanPlatformSupport::Initialize()
{

}

void RHIVulkanPlatformSupport::Cleanup()
{
}

VkFormat RHIVulkanPlatformSupport::GetVkFormat(RHIFormat InFormat)
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
	case R8G8B8A8_UNORM:
		return VK_FORMAT_R8G8B8A8_UNORM;
	default:
		std::runtime_error("UNIMPLEMENTED");
	}
	return VK_FORMAT_UNDEFINED;
}

VkDescriptorType RHIVulkanPlatformSupport::GetDescriptorType(DescriptorType Type)
{
	switch (Type)
	{
	case UNIFORM:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case SAMPLER2D:
		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	case IMAGE2D:
		return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	case STORAGE:
	case STORAGE_READONLY:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	}
	return VK_DESCRIPTOR_TYPE_SAMPLER;
}

VkImageStateDesc RHIVulkanPlatformSupport::GetStateDesc(RHIResourceState InState)
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

RHIVulkanContext::RHIVulkanContext() {}
RHIVulkanContext::~RHIVulkanContext() {}

void RHIVulkanContext::OnWindowResize(GLFWwindow* window, int width, int height)
{
	auto self = reinterpret_cast<RHIVulkanContext*>(glfwGetWindowUserPointer(window));
}

bool RHIVulkanContext::QuerySurfaceProperties()
{
	bool Result = PhysicalDeviceSupportSurface(SurfaceCapabilities, SurfaceFormats, PresentModes, CurrentPhysicalDevice.PhysicalDevice, Surface);
	return Result && SurfaceCapabilities.currentExtent.height > 0 && SurfaceCapabilities.currentExtent.width > 0;
}


void RHIVulkanContext::Initialize(uint32_t WindowWidth, uint32_t WindowHeight)
{
	CreateGLFWContext();
	VkDebugUtilsMessengerCreateInfoEXT DebugCreateInfo{};
	DebugCreateInfo.flags = 0;
	DebugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	DebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	DebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	DebugCreateInfo.pfnUserCallback = debugCallback;
	CreateVkInstance(Instance, DebugCreateInfo, DebugMessenger, InstanceExtensions);

	// Global Vulkan API Loader
	APILoader.Instance = Instance;

	if (vkCreateDebugUtilsMessengerEXT(Instance, &DebugCreateInfo, nullptr, &DebugMessenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
	PhysicalDeviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
		VK_EXT_DEPTH_RANGE_UNRESTRICTED_EXTENSION_NAME,
		VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME
	};
	RetrieveAvailablePhysicalDevices(AvailablePhysicalDevices, Instance, PhysicalDeviceExtensions);

	Log("All Physical Devices:");
	for (auto& PD : AvailablePhysicalDevices)
	{
		VkPhysicalDeviceProperties PDProp;
		vkGetPhysicalDeviceProperties(PD, &PDProp);
		Log("  ", PDProp.deviceName);
	}

	// Grab first physical device
	CurrentPhysicalDevice.PhysicalDevice = AvailablePhysicalDevices[0];
	vkGetPhysicalDeviceFeatures(CurrentPhysicalDevice.PhysicalDevice, &CurrentPhysicalDevice.PDFeatures);
	vkGetPhysicalDeviceProperties(CurrentPhysicalDevice.PhysicalDevice, &CurrentPhysicalDevice.PDProperties);
	vkGetPhysicalDeviceMemoryProperties(CurrentPhysicalDevice.PhysicalDevice, &CurrentPhysicalDevice.PDMemoryProperties);

	Log("Select Physical Device: ", CurrentPhysicalDevice.PDProperties.deviceName);

	// create glfw window
	CreateGLFWWindow(pGLFWwindow, WindowWidth, WindowHeight, this, OnWindowResize);
	CreateVkSurface(Instance, pGLFWwindow, Surface);
	SurfaceFormats.clear();
	PresentModes.clear();
	if (PhysicalDeviceSupportSurface(SurfaceCapabilities, SurfaceFormats, PresentModes, CurrentPhysicalDevice.PhysicalDevice, Surface)
		&& PhysicalDevideSupportQueueFamilies(CurrentPhysicalDevice.GraphicsQueueFamilyIndex,
			CurrentPhysicalDevice.PresentQueueFamilyIndex, CurrentPhysicalDevice.PhysicalDevice, Surface)
		&& CurrentPhysicalDevice.PDFeatures.samplerAnisotropy)
	{
	}
	else {
		Error("Physical Device does not support this window context! ");
	}

	assert(CurrentPhysicalDevice.PhysicalDevice != nullptr);
	CreateVkDevice(Device, GraphicsQueue, PresentQueue, CurrentPhysicalDevice.PhysicalDevice, 
		CurrentPhysicalDevice.GraphicsQueueFamilyIndex, CurrentPhysicalDevice.PresentQueueFamilyIndex, PhysicalDeviceExtensions);
	CreateCommandPool(FrameCommandPool, Device, CurrentPhysicalDevice.GraphicsQueueFamilyIndex);
	CreateCommandPool(TransientCommandPool, Device, CurrentPhysicalDevice.GraphicsQueueFamilyIndex);
}

void RHIVulkanContext::Cleanup()
{
	vkDestroySurfaceKHR(Instance, Surface, nullptr);
	glfwDestroyWindow(pGLFWwindow);
	glfwTerminate();
	vkDestroyCommandPool(Device, FrameCommandPool, nullptr);
	vkDestroyCommandPool(Device, TransientCommandPool, nullptr);
	vkDestroyDevice(Device, nullptr);
	vkDestroyInstance(Instance, nullptr);
}

void RHIVulkanContext::WaitDeviceIdle()
{
	vkDeviceWaitIdle(Device);
}

void RHIVulkanContext::ProcessFrameInput()
{
	glfwPollEvents();
}


bool RHIVulkanContext::IsWindowAlive()
{
	return !glfwWindowShouldClose(pGLFWwindow);
}


std::unique_ptr<IRHIContext> RHIVulkanPlatformSupport::CreateRHIContext()
{
	return std::make_unique<RHIVulkanContext>();
}

std::unique_ptr<IRHICommandBuffer     > RHIVulkanContext::CreateRHICommandBuffer      () { return std::make_unique<RHIVulkanCommandBuffer     >(); }
std::unique_ptr<IRHIFrameBuffer       > RHIVulkanContext::CreateRHIFrameBuffer        () { return std::make_unique<RHIVulkanFrameBuffer       >(); }
std::unique_ptr<IRHIImageResource     > RHIVulkanContext::CreateRHIImageResource      () { return std::make_unique<RHIVulkanImageResource     >(); }
std::unique_ptr<IRHIImGUI             > RHIVulkanContext::CreateRHIImGUI              () { return std::make_unique<RHIVulkanImGUI             >(); }
std::unique_ptr<IRHIPipelineFactory   > RHIVulkanContext::CreateRHIPipelineFactory    () { return std::make_unique<RHIVulkanPipelineFactory   >(); }
std::unique_ptr<IRHIPipelineObject    > RHIVulkanContext::CreateRHIPipelineObject     () { return std::make_unique<RHIVulkanPipelineObject    >(); }
std::unique_ptr<IRHIRenderPass        > RHIVulkanContext::CreateRHIRenderPass         () { return std::make_unique<RHIVulkanRenderPass        >(); }
std::unique_ptr<IRHISwapchain         > RHIVulkanContext::CreateRHISwapchain          () { return std::make_unique<RHIVulkanSwapchain         >(); }
std::unique_ptr<IRHIBuffer           > RHIVulkanContext::CreateRHIBuffer            () { return std::make_unique<RHIVulkanBuffer           >(); }


RHIVulkanSwapchain::RHIVulkanSwapchain()
{

}

RHIVulkanSwapchain::~RHIVulkanSwapchain()
{

}

void RHIVulkanSwapchain::Initialize(IRHIContext* Context, RHIFormat InSwapchainFormat)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	vkDeviceWaitIdle(VulkanContext->Device);
	SwapchainRHIFormat = InSwapchainFormat;
	SwapchainImageFormat = RHIVulkanPlatformSupport::GetVkFormat(InSwapchainFormat);
	DepthImageResource = std::make_unique<RHIVulkanImageResource>();
	CachedRenderPass = nullptr;

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(VulkanContext->Device, &semaphoreInfo, nullptr, &ImageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(VulkanContext->Device, &semaphoreInfo, nullptr, &RenderFinishSemaphore) != VK_SUCCESS ||
		vkCreateFence(VulkanContext->Device, &fenceInfo, nullptr, &InFlightFence) != VK_SUCCESS || 
		vkCreateSemaphore(VulkanContext->Device, &semaphoreInfo, nullptr, &TransitionFinishSemaphore) != VK_SUCCESS) {
		throw std::runtime_error("failed to create synchronization objects for a frame!");
	}
	Swapchain = nullptr;
	CreateSwapchain(
		Swapchain, SwapchainImages, SwapchainImageViews, SwapchainImageFormat, SwapchainExtent,
		VulkanContext->Device,
		VulkanContext->Surface,
		VulkanContext->SurfaceCapabilities,
		VulkanContext->SurfaceFormats,
		VulkanContext->PresentModes,
		VulkanContext->CurrentPhysicalDevice.GraphicsQueueFamilyIndex,
		VulkanContext->CurrentPhysicalDevice.PresentQueueFamilyIndex);
	DepthImageResource->Initialize(Context, { SwapchainExtent.width, SwapchainExtent.height, 1 }, RHIFormat::D32_SFLOAT, RHIResourceState::DEPTH_ATTACHMENT, 1);
	SwapchainFrameBuffers.resize(SwapchainImageViews.size());
	for (size_t i = 0; i < SwapchainImages.size(); i++) {
		SwapchainFrameBuffers[i] = new RHIVulkanFrameBuffer();
	}
	SwapchainAvailable = true;
}

void RHIVulkanSwapchain::Cleanup(IRHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	vkDestroySemaphore(VulkanContext->Device, ImageAvailableSemaphore, nullptr);
	vkDestroySemaphore(VulkanContext->Device, RenderFinishSemaphore, nullptr);
	vkDestroyFence(VulkanContext->Device, InFlightFence, nullptr);
}

void RHIVulkanSwapchain::AcquireFrame(IRHIContext* Context, IRHIFrameBuffer*& OutFrameBuffer, IRHIRenderPass* InRenderPass)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	RHIVulkanRenderPass* VkRenderPass = static_cast<RHIVulkanRenderPass*>(InRenderPass);
	if (!SwapchainAvailable)
	{
		if (!VulkanContext->QuerySurfaceProperties())
		{
			return;
		}
		DestroyFramebuffers(VulkanContext);
		Recreate(VulkanContext);
		CachedRenderPass = nullptr;
		if (!SwapchainAvailable)
		{
			return;
		}
	}

	if (VkRenderPass->RenderPass != CachedRenderPass)
	{
		CreateFramebuffers(VulkanContext, VkRenderPass->RenderPass);
		CachedRenderPass = VkRenderPass->RenderPass;
	}
	vkResetFences(VulkanContext->Device, 1, &InFlightFence);

	VkResult result = vkAcquireNextImageKHR(VulkanContext->Device, Swapchain,
		UINT64_MAX, ImageAvailableSemaphore, VK_NULL_HANDLE, &CurrentImageIndex);

	if (result==VK_ERROR_OUT_OF_DATE_KHR || result==VK_SUBOPTIMAL_KHR)
	{
		SwapchainAvailable = false;
		return;
	}
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	OutFrameBuffer = SwapchainFrameBuffers[CurrentImageIndex];

	RHIVulkanCommandBuffer TransitionCommandBuffer;
	// Transition swapchain image to presentable format
	auto ColorAttachmentDesc = RHIVulkanPlatformSupport::GetStateDesc(RHIResourceState::COLOR_ATTACHMENT);
	auto UnDefinedDesc = RHIVulkanPlatformSupport::GetStateDesc(RHIResourceState::UNDEFINED);
	TransitionCommandBuffer.Initialize(Context);
	TransitionCommandBuffer.BeginCommandBuffer();
	TransitionImageLayout(SwapchainImages[CurrentImageIndex], TransitionCommandBuffer.CommandBuffer, UnDefinedDesc.ImageLayout, ColorAttachmentDesc.ImageLayout,
		UnDefinedDesc.Access, ColorAttachmentDesc.Access,
		UnDefinedDesc.PipelineStage, ColorAttachmentDesc.PipelineStage, 1);
	TransitionCommandBuffer.EndCommandBuffer();
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &TransitionCommandBuffer.CommandBuffer;

	vkQueueSubmit(VulkanContext->GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(VulkanContext->GraphicsQueue);
}

void RHIVulkanSwapchain::PresentFrameAndRelease(IRHIContext* Context, IRHICommandBuffer* CommandBuffer)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	auto VkCmdBuf = static_cast<RHIVulkanCommandBuffer*>(CommandBuffer)->CommandBuffer;
	RHIVulkanCommandBuffer TransitionCommandBuffer;
	// Transition swapchain image to presentable format
	auto ColorAttachmentDesc = RHIVulkanPlatformSupport::GetStateDesc(RHIResourceState::COLOR_ATTACHMENT);
	auto PresentableDesc = RHIVulkanPlatformSupport::GetStateDesc(RHIResourceState::PRESENTABLE);
	TransitionCommandBuffer.Initialize(Context);
	TransitionCommandBuffer.BeginCommandBuffer();
	TransitionImageLayout(SwapchainImages[CurrentImageIndex], TransitionCommandBuffer.CommandBuffer, ColorAttachmentDesc.ImageLayout, PresentableDesc.ImageLayout,
		ColorAttachmentDesc.Access, PresentableDesc.Access,
		ColorAttachmentDesc.PipelineStage, PresentableDesc.PipelineStage, 1);
	TransitionCommandBuffer.EndCommandBuffer();

	{
		// submit recorded commandbuffer
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &ImageAvailableSemaphore;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &VkCmdBuf;

		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &RenderFinishSemaphore;


		if (vkQueueSubmit(VulkanContext->GraphicsQueue, 1, &submitInfo, InFlightFence) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}
	}

	VkSubmitInfo submitInfo{};
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &TransitionCommandBuffer.CommandBuffer;
	submitInfo.pWaitSemaphores = &RenderFinishSemaphore;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &TransitionFinishSemaphore;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pWaitDstStageMask = waitStages;
	vkQueueSubmit(VulkanContext->GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(VulkanContext->GraphicsQueue);
	{
		// present swapchain frame buffer
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &TransitionFinishSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &Swapchain;
		presentInfo.pImageIndices = &CurrentImageIndex;
		auto result = vkQueuePresentKHR(VulkanContext->PresentQueue, &presentInfo);
		if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			SwapchainAvailable = false;
			return;
		}
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

	}

}

void RHIVulkanSwapchain::CreateFramebuffers(RHIVulkanContext* VulkanContext, VkRenderPass VkRP)
{
	for (size_t i = 0; i < SwapchainImages.size(); i++) {
		RHIVulkanFrameBuffer* VulkanFB = static_cast<RHIVulkanFrameBuffer*>(SwapchainFrameBuffers[i]);
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = VkRP;
		framebufferInfo.attachmentCount = 1;//static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = &SwapchainImageViews[i];//attachments.data();
		framebufferInfo.width = SwapchainExtent.width;
		framebufferInfo.height = SwapchainExtent.height;
		framebufferInfo.layers = 1;
		if (DepthImageResource != nullptr) {
			std::array<VkImageView, 2> attachments = {
				SwapchainImageViews[i],
				DepthImageResource->DescriptorInfo.imageView
			};

			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();

			if (vkCreateFramebuffer(VulkanContext->Device, &framebufferInfo, nullptr, &VulkanFB->FrameBuffer) != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
		else {
			if (vkCreateFramebuffer(VulkanContext->Device, &framebufferInfo, nullptr, &VulkanFB->FrameBuffer) != VK_SUCCESS) {
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
		VulkanFB->Extent.height = SwapchainExtent.height;
		VulkanFB->Extent.width = SwapchainExtent.width;
		VulkanFB->Extent.depth = 1;
	}
}

void RHIVulkanSwapchain::DestroyFramebuffers(RHIVulkanContext* Context)
{
	for (size_t i = 0; i < SwapchainImages.size(); i++) {
		vkDestroyFramebuffer(Context->Device, static_cast<RHIVulkanFrameBuffer*>(SwapchainFrameBuffers[i])->FrameBuffer, nullptr);
	}
}

void RHIVulkanSwapchain::Recreate(RHIVulkanContext* Context)
{
	for (size_t i = 0; i < SwapchainImages.size(); i++) {
		vkDestroyImageView(Context->Device, SwapchainImageViews[i], nullptr);
	}
	vkDestroySwapchainKHR(Context->Device, Swapchain, nullptr);
	SwapchainAvailable = CreateSwapchain(
		Swapchain, SwapchainImages, SwapchainImageViews, SwapchainImageFormat, SwapchainExtent,
		Context->Device,
		Context->Surface,
		Context->SurfaceCapabilities,
		Context->SurfaceFormats,
		Context->PresentModes,
		Context->CurrentPhysicalDevice.GraphicsQueueFamilyIndex,
		Context->CurrentPhysicalDevice.PresentQueueFamilyIndex);

	if (SwapchainAvailable)
	{
		DepthImageResource->Cleanup(Context);
		DepthImageResource->Initialize(Context, { SwapchainExtent.width, SwapchainExtent.height, 1 }, RHIFormat::D32_SFLOAT, RHIResourceState::DEPTH_ATTACHMENT, 1);
		SwapchainAvailable = true;
	}
}


ImageExtent3D RHIVulkanSwapchain::GetFrameSize()
{
	return ImageExtent3D{ SwapchainExtent.width, SwapchainExtent.height, 1 };
}


void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}

// RHIVulkanCommandBuffer implementation
RHIVulkanCommandBuffer::~RHIVulkanCommandBuffer()
{
	// Cleanup should be called explicitly before destruction
	// But we'll clean up here as a safety measure
	if (CommandBuffer) {
		vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
		vkResetCommandPool(Device, CommandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
		vkDestroyCommandPool(Device, CommandPool, nullptr);
	}
}

void RHIVulkanCommandBuffer::Initialize(IRHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	Device = VulkanContext->Device;
	CreateCommandPool(CommandPool, Device, VulkanContext->CurrentPhysicalDevice.GraphicsQueueFamilyIndex);;
	CreateCommandBuffer(CommandBuffer, VulkanContext->Device, CommandPool);
}

void RHIVulkanCommandBuffer::Cleanup(IRHIContext* Context)
{
	if (CommandBuffer) {
		vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
		vkDestroyCommandPool(Device, CommandPool, nullptr);
	}
}

void RHIVulkanCommandBuffer::BeginCommandBuffer()
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(CommandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}
}

void RHIVulkanCommandBuffer::EndCommandBuffer()
{
	if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

void RHIVulkanCommandBuffer::ResetCommandBuffer()
{
	vkResetCommandBuffer(CommandBuffer, /*VkCommandBufferResetFlagBits*/ 0);
}