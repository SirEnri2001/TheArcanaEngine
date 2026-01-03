// RHIVulkanImpl.cpp - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
#include "RHIVulkanImpl.h"
#include "CoreLog.inl"

#include <algorithm>
#include <iostream>
#include <set>
#include <array>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "GLFW/glfw3.h"

// Implementation helpers
void CreateCommandBuffer(VkCommandBuffer& OutCommandBuffer, VkDevice Device, VkCommandPool CommandPool);

void CreateGLFWContext()
{
	glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
}

void CreateGLFWWindow(GLFWwindow*& pGLFWwindow, int width, int height, void* CallbackOwner, void (*framebufferResizeCallback)(GLFWwindow* window, int width, int height))
{
    pGLFWwindow = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(pGLFWwindow, CallbackOwner);
    glfwSetFramebufferSizeCallback(pGLFWwindow, framebufferResizeCallback);
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    if (messageSeverity== VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        Error("[Vulkan] ", pCallbackData->pMessage);
        __debugbreak();
    }else if (messageSeverity == VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        Warning("[Vulkan] ", pCallbackData->pMessage);
    }else
    {
        Log("[Vulkan] ", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

VkCommandBufferManaged::VkCommandBufferManaged(VkDevice InDevice, VkCommandPool InCommandPool)
    : Device(InDevice), CommandPool(InCommandPool)
{
    CreateCommandBuffer(CommandBuffer, Device, CommandPool);
}

VkCommandBufferManaged::~VkCommandBufferManaged() {
    vkFreeCommandBuffers(Device, CommandPool, 1, &CommandBuffer);
}

void CreateCommandBuffer(VkCommandBuffer& OutCommandBuffer, VkDevice Device, VkCommandPool CommandPool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(Device, &allocInfo, &OutCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void BeginCommandBufferOneTimeSubmit(VkCommandBuffer& InCommandBuffer, VkCommandPool commandPool, VkDevice device) {
    CreateCommandBuffer(InCommandBuffer, device, commandPool);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(InCommandBuffer, &beginInfo);
}

void EndCommandBufferOneTimeSubmit(VkCommandBuffer InCommandBuffer, VkCommandPool commandPool, VkQueue graphicsQueue, VkDevice device) {
    vkEndCommandBuffer(InCommandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &InCommandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &InCommandBuffer);
}

void CreateVkInstance(
    VkInstance& OutVkInstance,
    VkDebugUtilsMessengerCreateInfoEXT& OutDebugMsgCreateInfo,
    VkDebugUtilsMessengerEXT& OutDebugMessenger,
    std::vector<const char*>& InOutExtensions,
    PFN_vkDebugUtilsMessengerCallbackEXT DebugCallback
) {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    InOutExtensions = std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);

    InOutExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    InOutExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(InOutExtensions.size());
    createInfo.ppEnabledExtensionNames = InOutExtensions.data();

    if (DebugCallback)
    {

    }

    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
    createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&OutDebugMsgCreateInfo;


    if (vkCreateInstance(&createInfo, nullptr, &OutVkInstance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

void CreateVkSurface(VkInstance& Instance, GLFWwindow* Window, VkSurfaceKHR& OutVkSurface)
{
    if (glfwCreateWindowSurface(Instance, Window, nullptr, &OutVkSurface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

void CreateVkDevice(VkDevice& OutVkDevice, VkQueue& OutGraphicsQueue, VkQueue& OutPresentQueue,
    VkPhysicalDevice physicalDevice, uint32_t GraphicsFamilyIndex, uint32_t PresentFamilyIndex, const std::vector<const char*>& Extensions) {

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { GraphicsFamilyIndex, PresentFamilyIndex };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(Extensions.size());
    createInfo.ppEnabledExtensionNames = Extensions.data();

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &OutVkDevice) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(OutVkDevice, GraphicsFamilyIndex, 0, &OutGraphicsQueue);
    vkGetDeviceQueue(OutVkDevice, PresentFamilyIndex, 0, &OutPresentQueue);
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

void chooseSwapExtent(VkExtent2D& InOutExtent, const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        InOutExtent = capabilities.currentExtent;
        return;
    }
    InOutExtent.width = std::clamp(InOutExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    InOutExtent.height = std::clamp(InOutExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
}

void CreateImageView(VkImageView& OutImageView, VkImage image, VkDevice Device, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(Device, &viewInfo, nullptr, &OutImageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }
}

void CreateSwapchain(
    VkSwapchainKHR& OutSwapchain,
    std::vector<VkImage>& OutSwapchainImages,
    std::vector<VkImageView>& OutSwapchainImageViews,
    VkFormat& OutSwapchainImageFormat,
    VkExtent2D& InOutSwapchainExtent,
    const VkDevice& Device,
    const VkSurfaceKHR& Surface,
    const VkSurfaceCapabilitiesKHR& SurfaceCapabilities,
    const std::vector<VkSurfaceFormatKHR>& SurfaceFormats,
    const std::vector<VkPresentModeKHR>& PresentModes,
    uint32_t GraphicsFamilyIndex, uint32_t PresentFamilyIndex
) {
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(SurfaceFormats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(PresentModes);
    chooseSwapExtent(InOutSwapchainExtent, SurfaceCapabilities);

    uint32_t imageCount = SurfaceCapabilities.minImageCount + 1;
    if (SurfaceCapabilities.maxImageCount > 0 && imageCount > SurfaceCapabilities.maxImageCount) {
        imageCount = SurfaceCapabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = Surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = InOutSwapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queueFamilyIndices[] = { GraphicsFamilyIndex, PresentFamilyIndex };

    if (GraphicsFamilyIndex != PresentFamilyIndex) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = SurfaceCapabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(Device, &createInfo, nullptr, &OutSwapchain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(Device, OutSwapchain, &imageCount, nullptr);
    OutSwapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(Device, OutSwapchain, &imageCount, OutSwapchainImages.data());

    OutSwapchainImageFormat = surfaceFormat.format;
    OutSwapchainImageViews.clear();
    OutSwapchainImageViews.resize(imageCount);
    for (uint32_t i = 0; i < OutSwapchainImages.size(); i++) {
        CreateImageView(OutSwapchainImageViews[i], OutSwapchainImages[i], Device, OutSwapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
}

VkSampleCountFlagBits getMaxUsableSampleCount(const VkPhysicalDevice& physicalDevice) {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

void RetrieveAvailablePhysicalDevices(std::vector<VkPhysicalDevice>& OutAvailablePhysicalDevices, const VkInstance& instance, const std::vector<const char*>& extensions)
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> AllAvailablePhysicalDevices;
	AllAvailablePhysicalDevices.resize(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, AllAvailablePhysicalDevices.data());
    if(extensions.empty())
    {
        OutAvailablePhysicalDevices.insert(OutAvailablePhysicalDevices.end(), AllAvailablePhysicalDevices.begin(), AllAvailablePhysicalDevices.end());
	    return;
    }
    for(auto& Device : AllAvailablePhysicalDevices)
    {
	    if(PhysicalDeviceSupportExtensions(Device, extensions))
	    {
            OutAvailablePhysicalDevices.push_back(Device);
	    }
    }
}

bool PhysicalDevideSupportQueueFamilies(uint32_t& OutGraphicsFamily, uint32_t& OutPresentFamily, VkPhysicalDevice PhysicalDevice, VkSurfaceKHR surface)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevice, &queueFamilyCount, queueFamilies.data());
    int i = 0;
    bool SupportGraphicsFamily = false, SupportPresentFamilyForSurface = false;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            OutGraphicsFamily = i;
            SupportGraphicsFamily = true;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(PhysicalDevice, i, surface, &presentSupport);
        if (presentSupport) {
            OutPresentFamily = i;
            SupportPresentFamilyForSurface = true;
        }

        if (SupportGraphicsFamily && SupportPresentFamilyForSurface)
        {
            return true;
        }
        i++;
    }
    return false;
}

bool PhysicalDeviceSupportExtensions(VkPhysicalDevice device, std::vector<const char*> extensions) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(extensions.begin(), extensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

bool PhysicalDeviceSupportSurface(
    VkSurfaceCapabilitiesKHR& OutSurfaceCapabilities,
    std::vector<VkSurfaceFormatKHR>& OutSurfaceFormats,
    std::vector<VkPresentModeKHR>& OutPresentModes,
    VkPhysicalDevice PhysicalDevice, VkSurfaceKHR surface
) {

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, surface, &OutSurfaceCapabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        OutSurfaceFormats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, surface, &formatCount, OutSurfaceFormats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        OutPresentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, surface, &presentModeCount, OutPresentModes.data());
    }

    return formatCount > 0 && presentModeCount > 0;
}

void CreatePresentableRenderPass(VkRenderPass& OutRenderPass, VkDevice Device, bool hasDepth, VkFormat DepthFormat, 
    VkFormat SwapchainImageFormat, VkSampleCountFlagBits msaaSamples)
{
	VkAttachmentDescription colorAttachment{};
    colorAttachment.format = SwapchainImageFormat;
    colorAttachment.samples = msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = DepthFormat;
    depthAttachment.samples = msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    //VkAttachmentDescription colorAttachmentResolve{};
    //colorAttachmentResolve.format = SwapchainImageFormat;
    //colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    //colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    //colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    //colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    //colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    //colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    //VkAttachmentReference colorAttachmentResolveRef{};
    //colorAttachmentResolveRef.attachment = 2;
    //colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    if (hasDepth) {
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
    }
    //subpass.pResolveAttachments = &colorAttachmentResolveRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    //std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve 
    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = hasDepth ? 2 : 1;
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(Device, &renderPassInfo, nullptr, &OutRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}


// DepthAttachmentIndex -1 means no depth attachment
void CreateRenderPassSingleSubpass(VkRenderPass& OutRenderPass, VkDevice Device, const std::vector<VkAttachmentDescription>& Attachments, int32_t DepthAttachementIndex)
{
    std::vector<VkAttachmentReference> ColorAttachmentRefs;

    for(int i = 0; i < Attachments.size(); i++)
    {
        if(i==DepthAttachementIndex)
        {
	        continue;
        }
        ColorAttachmentRefs.push_back(VkAttachmentReference{static_cast<uint32_t>(i), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
    }

    VkAttachmentReference DepthAttachmentRef;
    if(DepthAttachementIndex>=0)
    {
        DepthAttachmentRef.attachment = DepthAttachementIndex;
	    DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = ColorAttachmentRefs.size();
    subpass.pColorAttachments = ColorAttachmentRefs.data();
    subpass.pDepthStencilAttachment = DepthAttachementIndex>0?&DepthAttachmentRef:nullptr;
    subpass.pResolveAttachments = nullptr;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = Attachments.size();
    renderPassInfo.pAttachments = Attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(Device, &renderPassInfo, nullptr, &OutRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void CreateDescriptorSetLayout(VkDescriptorSetLayout& OutDescriptorSetLayout, const std::vector<VkDescriptorSetLayoutBinding>& DescSetLayoutBindings, VkDevice Device)
{
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(DescSetLayoutBindings.size());
    layoutInfo.pBindings = DescSetLayoutBindings.data();
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT;

    if (vkCreateDescriptorSetLayout(Device, &layoutInfo, nullptr, &OutDescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void CreateGraphicsPipeline(VkPipeline& OutGraphicsPipeline, VkPipelineLayout& OutPipelineLayout, VkSampleCountFlagBits SampleCountFlagBits,
    VkDevice device, const std::vector<char>& VertShaderBytecode, const char* VertShaderMain, const std::vector<char>& FragShaderBytecode, const char* FragShaderMain,
    const std::vector<VkVertexInputBindingDescription>& BindingDescriptions, const std::vector<VkVertexInputAttributeDescription>& AttributeDescriptions,
    const VkDescriptorSetLayout& DescriptorSetLayout, const VkRenderPass& RenderPass) {

    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    // Create Shader Module for vertex and fragment shaders
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = VertShaderBytecode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(VertShaderBytecode.data());
        if (vkCreateShaderModule(device, &createInfo, nullptr, &vertShaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vertex shader module!");
        }

        createInfo.codeSize = FragShaderBytecode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(FragShaderBytecode.data());

        if (vkCreateShaderModule(device, &createInfo, nullptr, &fragShaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create fragment shader module!");
        }
    }

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = VertShaderMain;

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = FragShaderMain;

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    vertexInputInfo.vertexBindingDescriptionCount = BindingDescriptions.size();
    vertexInputInfo.vertexAttributeDescriptionCount = AttributeDescriptions.size();
    vertexInputInfo.pVertexBindingDescriptions = BindingDescriptions.data();
    vertexInputInfo.pVertexAttributeDescriptions = AttributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = SampleCountFlagBits;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    if (DescriptorSetLayout){
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = DescriptorSetLayout?1:0;
        pipelineLayoutInfo.pSetLayouts = DescriptorSetLayout?&DescriptorSetLayout:nullptr;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &OutPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }else
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &OutPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = OutPipelineLayout;
    pipelineInfo.renderPass = RenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &OutGraphicsPipeline);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}

void CreateCommandPool(VkCommandPool& OutCommandPool, VkDevice Device, uint32_t GraphcisFamilyIndex) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = GraphcisFamilyIndex;

    if (vkCreateCommandPool(Device, &poolInfo, nullptr, &OutCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics command pool!");
    }
}

void GetMemoryRequirement(VkMemoryRequirements& OutMemoryRequirement, VkDevice Device, VkImage Image)
{
    vkGetImageMemoryRequirements(Device, Image, &OutMemoryRequirement);
}

void GetMemoryRequirement(VkMemoryRequirements& OutMemoryRequirement, VkDevice Device, VkBuffer Buffer)
{
    vkGetBufferMemoryRequirements(Device, Buffer, &OutMemoryRequirement);
}

void CreateDeviceMemory(VkDeviceMemory& OutDeviceMemory, VkDevice Device, uint32_t Size, uint32_t MemoryTypeIndex)
{
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = Size;
    allocInfo.memoryTypeIndex = MemoryTypeIndex;

    if (vkAllocateMemory(Device, &allocInfo, nullptr, &OutDeviceMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }
}

void CreateImage(VkImage& Image, 
    VkDevice Device, VkExtent3D Extent, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = Extent;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = numSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(Device, &imageInfo, nullptr, &Image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }
}

void CreateBuffer(VkBuffer& OutBuffer, VkDevice Device, VkDeviceSize size, VkBufferUsageFlags usage) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(Device, &bufferInfo, nullptr, &OutBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }
}



void TransitionImageLayout(VkImage image, VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

void CopyBufferToImage(VkBuffer buffer, VkImage image, VkCommandBuffer commandBuffer, uint32_t width, uint32_t height) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void CreateSampler(VkSampler& OutSampler, VkDevice Device, float maxSamplerAnisotropy) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    samplerInfo.mipLodBias = 0.0f;

    if (vkCreateSampler(Device, &samplerInfo, nullptr, &OutSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void CopyBuffer(VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize Size, VkCommandBuffer CommandBuffer)
{
    VkBufferCopy copyRegion{};
    copyRegion.size = Size;
    vkCmdCopyBuffer(CommandBuffer, SrcBuffer, DstBuffer, 1, &copyRegion);
}


void CreateDescriptorPool(VkDescriptorPool& OutDescriptorPool, VkDevice Device, uint32_t UniformBufferCount, uint32_t CombinedImageSamplerCount) {
    std::vector<VkDescriptorPoolSize> poolSizes{};

    VkDescriptorPoolSize UniformDescPoolSize{};
    UniformDescPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    UniformDescPoolSize.descriptorCount = UniformBufferCount;
    poolSizes.push_back(UniformDescPoolSize);

    VkDescriptorPoolSize SamplerDescPoolSize{};
    SamplerDescPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    SamplerDescPoolSize.descriptorCount = CombinedImageSamplerCount;
    poolSizes.push_back(SamplerDescPoolSize);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 16;

    if (vkCreateDescriptorPool(Device, &poolInfo, nullptr, &OutDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void CreateDescriptorSet(VkDescriptorSet& OutDescriptorSet, std::vector<VkWriteDescriptorSet>& InOutWriteDescriptorSets, VkDevice Device, VkDescriptorPool DescriptorPool, const VkDescriptorSetLayout& DescriptorSetLayout) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &DescriptorSetLayout;

    if (vkAllocateDescriptorSets(Device, &allocInfo, &OutDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (auto& WriteDesc : InOutWriteDescriptorSets)
    {
        WriteDesc.dstSet = OutDescriptorSet;
    }

    vkUpdateDescriptorSets(Device, InOutWriteDescriptorSets.size(), InOutWriteDescriptorSets.data(), 0, nullptr);
}

void CreateMipmapForImage(VkCommandBuffer& InCommandBuffer, VkImage Image, int32_t TexWidth, int32_t TexHeight, uint32_t MipmapLevel)
{
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = Image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	for (uint32_t i = 1; i < MipmapLevel; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(InCommandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { TexWidth, TexHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { TexWidth > 1 ? TexWidth / 2 : 1, TexHeight > 1 ? TexHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(InCommandBuffer,
			Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		//barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		//barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		//barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		//barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		//vkCmdPipelineBarrier(InCommandBuffer,
		//	VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		//	0, nullptr,
		//	0, nullptr,
		//	1, &barrier);

		if (TexWidth > 1) TexWidth /= 2;
		if (TexHeight > 1) TexHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = MipmapLevel - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	vkCmdPipelineBarrier(InCommandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

}
