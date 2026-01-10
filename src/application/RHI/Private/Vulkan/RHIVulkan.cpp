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
	return static_cast<RHIVulkanPlatformSupport*>(IRHIPlatformSupport::Get());
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

void RHIVulkanContext::Initialize(IRHIPlatformSupport* PlatformSupport)
{
	auto* VulkanPlatformSupport = static_cast<RHIVulkanPlatformSupport*>(PlatformSupport);
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
std::unique_ptr<IRHIContext> RHIVulkanPlatformSupport::CreateRHIContext()
{
	return std::make_unique<RHIVulkanContext>();
}

std::unique_ptr<IRHIBufferResource    > RHIVulkanContext::CreateRHIBufferResource     () { return std::make_unique<RHIVulkanBufferResource    >(); }
std::unique_ptr<IRHICommandBuffer     > RHIVulkanContext::CreateRHICommandBuffer      () { return std::make_unique<RHIVulkanCommandBuffer     >(); }
std::unique_ptr<IRHIComputeDispatcher > RHIVulkanContext::CreateRHIComputeDispatcher  () { return std::make_unique<RHIVulkanComputeDispatcher >(); }
std::unique_ptr<IRHIFrameBuffer       > RHIVulkanContext::CreateRHIFrameBuffer        () { return std::make_unique<RHIVulkanFrameBuffer       >(); }
std::unique_ptr<IRHIGraphicsDispatcher> RHIVulkanContext::CreateRHIGraphicsDispatcher () { return std::make_unique<RHIVulkanGraphicsDispatcher>(); }
std::unique_ptr<IRHIImageResource     > RHIVulkanContext::CreateRHIImageResource      () { return std::make_unique<RHIVulkanImageResource     >(); }
std::unique_ptr<IRHIImGUI             > RHIVulkanContext::CreateRHIImGUI              () { return std::make_unique<RHIVulkanImGUI             >(); }
std::unique_ptr<IRHIPipelineFactory   > RHIVulkanContext::CreateRHIPipelineFactory    () { return std::make_unique<RHIVulkanPipelineFactory   >(); }
std::unique_ptr<IRHIPipelineObject    > RHIVulkanContext::CreateRHIPipelineObject     () { return std::make_unique<RHIVulkanPipelineObject    >(); }
std::unique_ptr<IRHIPresentPass       > RHIVulkanContext::CreateRHIPresentPass        () { return std::make_unique<RHIVulkanPresentPass       >(); }
std::unique_ptr<IRHIRenderPass        > RHIVulkanContext::CreateRHIRenderPass         () { return std::make_unique<RHIVulkanRenderPass        >(); }
std::unique_ptr<IRHISwapchain         > RHIVulkanContext::CreateRHISwapchain          () { return std::make_unique<RHIVulkanSwapchain         >(); }
std::unique_ptr<IRHIUniform           > RHIVulkanContext::CreateRHIUniform            () { return std::make_unique<RHIVulkanUniform           >(); }
std::unique_ptr<IRHIWindowManager     > RHIVulkanContext::CreateRHIWindowManager      () { return std::make_unique<RHIVulkanWindowManager     >(); }

void RHIVulkanWindowManager::Initialize(IRHIPlatformSupport* InPlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth)
{
	auto* VulkanPlatformSupport = static_cast<RHIVulkanPlatformSupport*>(InPlatformSupport);
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

void RHIVulkanWindowManager::CleanupSwapchain(IRHIContext* Context)
{
//	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
//	for (auto* ImageResource : SwapchainImageResources) {
//		auto* VulkanImgRes = static_cast<RHIVulkanImageResource*>(ImageResource);
//		vkDestroyImageView(VulkanContext->Device, VulkanImgRes->DescriptorInfo.imageView, nullptr);
//	}
//	vkDestroySwapchainKHR(VulkanContext->Device, CurrentSwapchain.Swapchain, nullptr);
//	DepthImageResource->Cleanup(Context);
}

void RHIVulkanWindowManager::InitializeSwapchain(IRHIContext* Context, IRHIPlatformSupport* PlatformSupport)
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

void RHIVulkanWindowManager::InitializeRenderPassAsPresent(IRHIRenderPass* OutRenderPass, IRHIContext* Context)
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


void RHIVulkanWindowManager::RecreateSwapchain(IRHIContext* Context)
{
//	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
//	CleanupSwapchain(Context);
//	InitializeSwapchain(Context, PlatformSupport);
}

void RHIVulkanWindowManager::AddScreenSizeTexture(IRHIImageResource* ImageResource)
{
	auto* VkImage = static_cast<RHIVulkanImageResource*>(ImageResource);
	ScreenSizeImages.push_back(VkImage);
}

void RHIVulkanWindowManager::RemoveScreenSizeTexture(IRHIImageResource* ImageResource)
{
	auto* VkImage = static_cast<RHIVulkanImageResource*>(ImageResource);
	ScreenSizeImages.erase(std::remove(ScreenSizeImages.begin(), ScreenSizeImages.end(), VkImage), ScreenSizeImages.end());
}


void RHIVulkanWindowManager::OnWindowResize(GLFWwindow* window, int width, int height)
{
	auto self = reinterpret_cast<RHIVulkanContext*>(glfwGetWindowUserPointer(window));
	std::cout<<"Windows Resize!\n";
}

void RHIVulkanWindowManager::Cleanup(IRHIPlatformSupport* PlatformSupport)
{
	auto* VulkanPlatformSupport = static_cast<RHIVulkanPlatformSupport*>(PlatformSupport);
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

void RHIVulkanSwapchain::Initialize(IRHIContext* Context, IRHIWindowManager* WindowManager)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	auto* VulkanWindow = static_cast<RHIVulkanWindowManager*>(WindowManager);
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
		SwapchainFrameBuffers[i] = new RHIVulkanFrameBuffer();
	}
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

void RHIVulkanCommandBuffer::Initialize(IRHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	Device = VulkanContext->Device;
	CommandPool = VulkanContext->CommandPool;
	CreateCommandBuffer(CommandBuffer, VulkanContext->Device, VulkanContext->CommandPool);
}

void RHIVulkanCommandBuffer::Cleanup(IRHIContext* Context)
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
void RHIVulkanComputeDispatcher::Initialize(IRHIContext* Context)
{
	// No special initialization needed for compute dispatcher
}

void RHIVulkanComputeDispatcher::Cleanup(IRHIContext* Context)
{
	// No special cleanup needed for compute dispatcher
}

void RHIVulkanComputeDispatcher::Dispatch(IRHICommandBuffer* CommandBuffer, IRHIPipelineObject* PipelineObject, uint32_t ThreadGroupX, uint32_t ThreadGroupY, uint32_t ThreadGroupZ)
{
	auto* VulkanCommandBuffer = static_cast<RHIVulkanCommandBuffer*>(CommandBuffer);
	VkCommandBuffer VkCmdBuf = VulkanCommandBuffer->CommandBuffer;
	auto* VulkanPipeline = static_cast<RHIVulkanPipelineObject*>(PipelineObject);
	
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

void RHIVulkanComputeDispatcher::WaitForGPUIdle(IRHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	vkDeviceWaitIdle(VulkanContext->Device);
}
