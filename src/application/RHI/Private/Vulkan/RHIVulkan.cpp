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
}

void RHIVulkanPlatformSupport::Cleanup()
{
	vkDestroyInstance(Instance, nullptr);
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
	SurfaceFormats.clear();
	PresentModes.clear();
	if (PhysicalDeviceSupportSurface(SurfaceCapabilities, SurfaceFormats, PresentModes, VulkanPlatformSupport->CurrentPhysicalDevice.PhysicalDevice, Surface)
		&& PhysicalDevideSupportQueueFamilies(VulkanPlatformSupport->CurrentPhysicalDevice.GraphicsQueueFamilyIndex, 
			VulkanPlatformSupport->CurrentPhysicalDevice.PresentQueueFamilyIndex, VulkanPlatformSupport->CurrentPhysicalDevice.PhysicalDevice, Surface)
		&& VulkanPlatformSupport->CurrentPhysicalDevice.PDFeatures.samplerAnisotropy)
	{
		return;
	}
	else {
		Error("Physical Device does not support this window context! ");
	}
}

void RHIVulkanWindowManager::CleanupSwapchain(RHIContext* Context)
{
//	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
//	for (auto* ImageResource : SwapchainImageResources) {
//		auto* VulkanImgRes = static_cast<RHIVulkanImageResource*>(ImageResource);
//		vkDestroyImageView(VulkanContext->Device, VulkanImgRes->DescriptorInfo.imageView, nullptr);
//	}
//	vkDestroySwapchainKHR(VulkanContext->Device, CurrentSwapchain.Swapchain, nullptr);
//	DepthImageResource->Cleanup(Context);
}

void RHIVulkanWindowManager::InitializeSwapchain(RHIContext* Context, RHIPlatformSupport* PlatformSupport)
{
//	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
//	auto* VulkanPlatformSupport = static_cast<RHIVulkanPlatformSupport*>(PlatformSupport->GetImpl());
//
//	std::vector<VkImage> SwapchainImages;
//	std::vector<VkImageView> SwapchainImageViews;
//	VkFormat SwapchainImageFormat;
//	glfwGetFramebufferSize(pGLFWwindow, reinterpret_cast<int*>(&SwapchainExtent.width), reinterpret_cast<int*>(&SwapchainExtent.height));
//	while (SwapchainExtent.width == 0 || SwapchainExtent.height == 0) {
//		glfwGetFramebufferSize(pGLFWwindow, reinterpret_cast<int*>(&SwapchainExtent.width), reinterpret_cast<int*>(&SwapchainExtent.height));
//		glfwWaitEvents();
//	}
//
//	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VulkanPlatformSupport->CurrentPhysicalDevice.PhysicalDevice, Surface, &SurfaceCapabilities);
}

void RHIVulkanWindowManager::InitializeRenderPassAsPresent(RHIRenderPass* OutRenderPass, RHIContext* Context)
{
	//auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	//auto* VulkanRenderPass = static_cast<RHIVulkanRenderPass*>(OutRenderPass->GetImpl());
	//PresentRenderPass = OutRenderPass;
	//VulkanRenderPass->DepthRenderTargets = CurrentSwapchain.DepthRT;
	//CreatePresentableRenderPass(VulkanRenderPass->RenderPass, VulkanContext->Device, VulkanRenderPass->DepthRenderTargets != nullptr, RHIVulkanPlatformSupport::Get()->GetDepthFormat(), CurrentSwapchain.SwapchainImageFormat, VK_SAMPLE_COUNT_1_BIT);
	//CurrentSwapchain.SwapchainFramebuffers.resize(CurrentSwapchain.SwapchainImageViews.size());
	//for (size_t i = 0; i < CurrentSwapchain.SwapchainImageViews.size(); i++) {
	//	VkFramebufferCreateInfo framebufferInfo{};
	//	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	//	framebufferInfo.renderPass = VulkanRenderPass->RenderPass;
	//	framebufferInfo.attachmentCount = 1;//static_cast<uint32_t>(attachments.size());
	//	framebufferInfo.pAttachments = &CurrentSwapchain.SwapchainImageViews[i];//attachments.data();

	//	framebufferInfo.width = CurrentSwapchain.SwapchainExtent.width;
	//	framebufferInfo.height = CurrentSwapchain.SwapchainExtent.height;
	//	framebufferInfo.layers = 1;
	//	if (VulkanRenderPass->DepthRenderTargets != nullptr) {
	//		std::array<VkImageView, 2> attachments = {
	//			CurrentSwapchain.SwapchainImageViews[i],
 //               VulkanRenderPass->DepthRenderTargets->DescriptorInfo.imageView
	//		};

	//		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	//		framebufferInfo.pAttachments = attachments.data();

	//		if (vkCreateFramebuffer(VulkanContext->Device, &framebufferInfo, nullptr, &CurrentSwapchain.SwapchainFramebuffers[i]) != VK_SUCCESS) {
	//			throw std::runtime_error("failed to create framebuffer!");
	//		}
	//	}
	//	else {
	//		if (vkCreateFramebuffer(VulkanContext->Device, &framebufferInfo, nullptr, &CurrentSwapchain.SwapchainFramebuffers[i]) != VK_SUCCESS) {
	//			throw std::runtime_error("failed to create framebuffer!");
	//		}
	//	}
	//}
}


void RHIVulkanWindowManager::RecreateSwapchain(RHIContext* Context)
{
//	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
//	CleanupSwapchain(Context);
//	InitializeSwapchain(Context, PlatformSupport);
}

void RHIVulkanWindowManager::AddScreenSizeTexture(RHIImageResource* ImageResource)
{
	auto* VkImage = static_cast<RHIVulkanImageResource*>(ImageResource->GetImpl());
	ScreenSizeImages.push_back(VkImage);
}

void RHIVulkanWindowManager::RemoveScreenSizeTexture(RHIImageResource* ImageResource)
{
	auto* VkImage = static_cast<RHIVulkanImageResource*>(ImageResource->GetImpl());
	ScreenSizeImages.erase(std::remove(ScreenSizeImages.begin(), ScreenSizeImages.end(), VkImage), ScreenSizeImages.end());
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


RHIVulkanSwapchain::RHIVulkanSwapchain()
{

}

RHIVulkanSwapchain::~RHIVulkanSwapchain()
{

}

void RHIVulkanSwapchain::Initialize(RHIContext* Context, RHIWindowManager* WindowManager)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VulkanWindow = static_cast<RHIVulkanWindowManager*>(WindowManager->GetImpl());
	vkDeviceWaitIdle(VulkanContext->Device);
	CreateSwapchain(
		Swapchain, SwapchainImages, SwapchainImageViews, SwapchainImageFormat, SwapchainExtent,
		VulkanContext->Device, 
		VulkanWindow->Surface, 
		VulkanWindow->SurfaceCapabilities, 
		VulkanWindow->SurfaceFormats, 
		VulkanWindow->PresentModes,
		RHIVulkanPlatformSupport::Get()->CurrentPhysicalDevice.GraphicsQueueFamilyIndex,
		RHIVulkanPlatformSupport::Get()->CurrentPhysicalDevice.PresentQueueFamilyIndex);
	DepthImageResource = new RHIVulkanImageResource();
	DepthImageResource->Initialize(Context, { SwapchainExtent.width, SwapchainExtent.height, 1 }, RHIFormat::D32_SFLOAT, RHIResourceState::DEPTH_ATTACHMENT, 1);
	SwapchainFrameBuffers.resize(SwapchainImageViews.size());
	for (size_t i = 0; i < SwapchainImages.size(); i++) {
		SwapchainFrameBuffers[i] = new RHIFrameBuffer();
	}
	CachedRenderPass = nullptr;

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateSemaphore(VulkanContext->Device, &semaphoreInfo, nullptr, &ImageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(VulkanContext->Device, &semaphoreInfo, nullptr, &RenderFinishSemaphore) != VK_SUCCESS ||
		vkCreateFence(VulkanContext->Device, &fenceInfo, nullptr, &InFlightFence) != VK_SUCCESS) {
		throw std::runtime_error("failed to create synchronization objects for a frame!");
	}
}

void RHIVulkanSwapchain::Cleanup(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	vkDestroySemaphore(VulkanContext->Device, ImageAvailableSemaphore, nullptr);
	vkDestroySemaphore(VulkanContext->Device, RenderFinishSemaphore, nullptr);
	vkDestroyFence(VulkanContext->Device, InFlightFence, nullptr);
}

void RHIVulkanSwapchain::AcquireFrame(RHIContext* Context, RHIFrameBuffer*& OutFrameBuffer, RHIRenderPass* InRenderPass)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	RHIVulkanRenderPass* VkRenderPass = static_cast<RHIVulkanRenderPass*>(InRenderPass->GetImpl());
	if (VkRenderPass->RenderPass!=CachedRenderPass)
	{
		CreateFramebuffers(VulkanContext, VkRenderPass->RenderPass);
		CachedRenderPass = VkRenderPass->RenderPass;
	}
	vkResetFences(VulkanContext->Device, 1, &InFlightFence);
	VkResult result = vkAcquireNextImageKHR(VulkanContext->Device, Swapchain,
		UINT64_MAX, ImageAvailableSemaphore, VK_NULL_HANDLE, &CurrentImageIndex);

	if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	OutFrameBuffer = SwapchainFrameBuffers[CurrentImageIndex];
}

void RHIVulkanSwapchain::PresentFrameAndRelease(RHIContext* Context, RHICommandBuffer* CommandBuffer, RHIGraphicsDispatcher* GDispatcher)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	auto* VkDispatcher = static_cast<RHIVulkanGraphicDispatcher*>(GDispatcher->GetImpl());
	auto VkCmdBuf = static_cast<RHIVulkanCommandBuffer*>(CommandBuffer->GetImpl())->CommandBuffer;

	// Transition swapchain image to presentable format
	auto ColorAttachmentDesc = RHIVulkanPlatformSupport::GetStateDesc(RHIResourceState::COLOR_ATTACHMENT);
	auto PresentableDesc = RHIVulkanPlatformSupport::GetStateDesc(RHIResourceState::PRESENTABLE);
	TransitionImageLayout(SwapchainImages[CurrentImageIndex], VkCmdBuf, ColorAttachmentDesc.ImageLayout, PresentableDesc.ImageLayout,
		ColorAttachmentDesc.Access, PresentableDesc.Access,
		ColorAttachmentDesc.PipelineStage, PresentableDesc.PipelineStage, 1);

	if (vkEndCommandBuffer(VkCmdBuf) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}

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

	{
		// present swapchain frame buffer
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &RenderFinishSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &Swapchain;
		presentInfo.pImageIndices = &CurrentImageIndex;
		auto result = vkQueuePresentKHR(VulkanContext->PresentQueue, &presentInfo);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}
	}

}

void RHIVulkanSwapchain::CreateFramebuffers(RHIVulkanContext* VulkanContext, VkRenderPass VkRP)
{
	for (size_t i = 0; i < SwapchainImages.size(); i++) {
		RHIVulkanFrameBuffer* VulkanFB = static_cast<RHIVulkanFrameBuffer*>(SwapchainFrameBuffers[i]->GetImpl());
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

// RHIVulkanCommandBuffer implementation
RHIVulkanCommandBuffer::~RHIVulkanCommandBuffer()
{
	// Cleanup should be called explicitly before destruction
	// But we'll clean up here as a safety measure
	if (CommandBuffer) {
		vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
	}
}

void RHIVulkanCommandBuffer::Initialize(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	Device = VulkanContext->Device;
	CommandPool = VulkanContext->CommandPool;
	CreateCommandBuffer(CommandBuffer, VulkanContext->Device, VulkanContext->CommandPool);
}

void RHIVulkanCommandBuffer::Cleanup(RHIContext* Context)
{
	if (CommandBuffer) {
		vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
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

// RHIVulkanComputeDispatcher implementation
void RHIVulkanComputeDispatcher::Initialize(RHIContext* Context)
{
	// No special initialization needed for compute dispatcher
}

void RHIVulkanComputeDispatcher::Cleanup(RHIContext* Context)
{
	// No special cleanup needed for compute dispatcher
}

void RHIVulkanComputeDispatcher::Dispatch(RHICommandBuffer* CommandBuffer, RHIPipelineObject* PipelineObject, uint32_t ThreadGroupX, uint32_t ThreadGroupY, uint32_t ThreadGroupZ)
{
	auto* VulkanCommandBuffer = static_cast<RHIVulkanCommandBuffer*>(CommandBuffer->GetImpl());
	VkCommandBuffer VkCmdBuf = VulkanCommandBuffer->CommandBuffer;
	auto* VulkanPipeline = static_cast<RHIVulkanPipelineObject*>(PipelineObject->GetImpl());
	
	// Bind compute pipeline
	vkCmdBindPipeline(VkCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, VulkanPipeline->Pipeline);
	
	// Push descriptors if needed
	if (VulkanPipeline->WriteDescriptorSets.size() > 0)
	{
		vkCmdPushDescriptorSetKHR(VkCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, VulkanPipeline->PipelineLayout,
			0, VulkanPipeline->WriteDescriptorSets.size(), VulkanPipeline->WriteDescriptorSets.data());
	}
	
	// Dispatch compute shader
	vkCmdDispatch(VkCmdBuf, ThreadGroupX, ThreadGroupY, ThreadGroupZ);
}

void RHIVulkanComputeDispatcher::WaitForGPUIdle(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	vkDeviceWaitIdle(VulkanContext->Device);
}
