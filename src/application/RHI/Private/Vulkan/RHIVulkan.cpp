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

#include "RHIVulkanImpl.h"
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
