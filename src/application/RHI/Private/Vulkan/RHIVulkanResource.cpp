#include <cmath>

#define RHI_IMPLEMENT
#include "RHIVulkan.h"
#include "RHIVulkanImpl.h"

void RHIVulkanImageResource::InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage, uint32_t MultiSamplesCount)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	VkImageUsageFlags VkImageUsage;
	VkImageAspectFlagBits VkImageAspectFlagBits;
	Usage = InUsage;
	switch (Usage)
	{
	case IU_COLOR_RT:
		InnerFormat = VK_FORMAT_B8G8R8A8_SRGB;
		VkImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT;
		DescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		CreateSampler(DescriptorInfo.sampler, VulkanContext->Device, RHIVulkanPlatformSupport::Get()->CurrentPhysicalDevice.PDProperties.limits.maxSamplerAnisotropy);
		bHasSampler = true;
		// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL
		break;
	case IU_COLOR_PRESENT_RT:
		InnerFormat = VK_FORMAT_B8G8R8A8_SRGB;
		VkImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT;
		DescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		break;
	case IU_DEPTH_RT:
		InnerFormat = RHIVulkanPlatformSupport::Get()->GetDepthFormat();
		VkImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_DEPTH_BIT;
		DescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		break;
	default:
		throw std::runtime_error("Invalid usage bit for init a rendertarget");
	}
	ImageExtent.height = RTExtent.Height;
	ImageExtent.width = RTExtent.Width;
	ImageExtent.depth = RTExtent.Depth;
	CreateImage(Image, VulkanContext->Device, ImageExtent, 1, static_cast<VkSampleCountFlagBits>(MultiSamplesCount), InnerFormat, VK_IMAGE_TILING_OPTIMAL, VkImageUsage);
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(VulkanContext->Device, Image, &memRequirements);
	CreateDeviceMemory(DeviceMemory, VulkanContext->Device, memRequirements.size, RHIVulkanPlatformSupport::Get()->GetMemoryType(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	vkBindImageMemory(VulkanContext->Device, Image, DeviceMemory, 0);
	CreateImageView(DescriptorInfo.imageView, Image, VulkanContext->Device, InnerFormat, VkImageAspectFlagBits, 1);
}


void RHIVulkanImageResource::Initialize(RHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, ImageUsage InUsage, int32_t InMipLevel)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	Usage = IU_GENERAL;
	InnerFormat = RHIVulkanPlatformSupport::GetVkFormat(InFormat);
	// Check if image format supports linear blitting
	VkFormatProperties formatProperties = RHIVulkanPlatformSupport::Get()->GetFormatProperties(InnerFormat);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}
	if(InMipLevel==-1)
	{
		InMipLevel = static_cast<uint32_t>(std::floor(std::log2(std::max(Width, Height)))) + 1;
	}
	MipLevel = InMipLevel;
	{
		ImageExtent.height = Height;
		ImageExtent.width = Width;
		ImageExtent.depth = 1;
		CreateImage(Image, VulkanContext->Device, ImageExtent, InMipLevel, VK_SAMPLE_COUNT_1_BIT, InnerFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(VulkanContext->Device, Image, &memRequirements);
		CreateDeviceMemory(DeviceMemory, VulkanContext->Device, memRequirements.size, RHIVulkanPlatformSupport::Get()->GetMemoryType(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
		vkBindImageMemory(VulkanContext->Device, Image, DeviceMemory, 0);
		CreateImageView(DescriptorInfo.imageView, Image, VulkanContext->Device, InnerFormat, VK_IMAGE_ASPECT_COLOR_BIT, InMipLevel);
		CreateSampler(DescriptorInfo.sampler, VulkanContext->Device, RHIVulkanPlatformSupport::Get()->CurrentPhysicalDevice.PDProperties.limits.maxSamplerAnisotropy);
	}
	bHasSampler = true;
	DescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}


void RHIVulkanImageResource::Initialize(RHIContext* Context, ImageExtent3D RTExtent, RHIFormat InFormat, ImageUsage InUsage, int32_t InMipLevel)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	bHasSampler = InUsage != ImageUsage::IU_COLOR_PRESENT_RT && InUsage != ImageUsage::IU_DPETH_PRESENT_RT;
	Usage = InUsage;
	InnerFormat = RHIVulkanPlatformSupport::GetVkFormat(InFormat);
	ImageExtent = VkExtent3D{RTExtent.Width, RTExtent.Height, RTExtent.Depth};
	MipLevel = bHasSampler ? InMipLevel : 1;

	// Check if image format supports linear blitting
	VkFormatProperties formatProperties = RHIVulkanPlatformSupport::Get()->GetFormatProperties(InnerFormat);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	VkImageUsageFlags VkImageUsage;
	VkImageAspectFlagBits VkImageAspectFlagBits;
	switch (Usage)
	{
	case IU_COLOR_RT:
		VkImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT;
		DescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		// VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL
		break;
	case IU_COLOR_PRESENT_RT:
		VkImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT;
		DescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		break;
	case IU_DEPTH_RT:
	case IU_DPETH_PRESENT_RT:
		VkImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_DEPTH_BIT;
		DescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		break;
	case IU_GENERAL:
		VkImageUsage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT;
		break;
	default:
		throw std::runtime_error("Invalid usage bit for init a rendertarget");
	}
	CreateImage(Image, VulkanContext->Device, ImageExtent, 1, VK_SAMPLE_COUNT_1_BIT, InnerFormat, VK_IMAGE_TILING_OPTIMAL, VkImageUsage);
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(VulkanContext->Device, Image, &memRequirements);
	CreateDeviceMemory(DeviceMemory, VulkanContext->Device, memRequirements.size, RHIVulkanPlatformSupport::Get()->GetMemoryType(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	vkBindImageMemory(VulkanContext->Device, Image, DeviceMemory, 0);
	CreateImageView(DescriptorInfo.imageView, Image, VulkanContext->Device, InnerFormat, VkImageAspectFlagBits, 1);
	if (bHasSampler)
	{
		CreateSampler(DescriptorInfo.sampler, VulkanContext->Device, RHIVulkanPlatformSupport::Get()->CurrentPhysicalDevice.PDProperties.limits.maxSamplerAnisotropy);
	}
}

VkDescriptorImageInfo& RHIVulkanImageResource::GetDescriptorImageInfo() {

	static VkDescriptorImageInfo Desc;
	Desc = DescriptorInfo;
	return Desc;
}

void RHIVulkanImageResource::CopyToTexture(RHIContext* Context, void* Data, uint32_t Stride)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	VkDeviceSize imageSize = ImageExtent.width * ImageExtent.height * Stride;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(VulkanContext->Device, Image, &memRequirements);
	{
		CreateBuffer(stagingBuffer, VulkanContext->Device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	    vkGetBufferMemoryRequirements(VulkanContext->Device, stagingBuffer, &memRequirements);
		CreateDeviceMemory(stagingBufferMemory, VulkanContext->Device, memRequirements.size, RHIVulkanPlatformSupport::Get()->GetMemoryType(memRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
		vkBindBufferMemory(VulkanContext->Device, stagingBuffer, stagingBufferMemory, 0);

		void* DstData;
		vkMapMemory(VulkanContext->Device, stagingBufferMemory, 0, imageSize, 0, &DstData);
		memcpy(DstData, Data, static_cast<size_t>(imageSize));
		vkUnmapMemory(VulkanContext->Device, stagingBufferMemory);
	}

	VkCommandBuffer commandBuffer;
	CreateCommandBuffer(commandBuffer, VulkanContext->Device, VulkanContext->CommandPool);
	BeginCommandBufferOneTimeSubmit(commandBuffer, VulkanContext->CommandPool, VulkanContext->Device);
	TransitionImageLayout(Image, commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, MipLevel);
	CopyBufferToImage(stagingBuffer, Image, commandBuffer, ImageExtent.width, ImageExtent.height);
	CreateMipmapForImage(commandBuffer, Image, ImageExtent.width, ImageExtent.height, MipLevel);
	TransitionImageLayout(Image, commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, MipLevel);
	EndCommandBufferOneTimeSubmit(commandBuffer, VulkanContext->CommandPool, VulkanContext->GraphicsQueue, VulkanContext->Device);

	vkDestroyBuffer(VulkanContext->Device, stagingBuffer, nullptr);
	vkFreeMemory(VulkanContext->Device, stagingBufferMemory, nullptr);
}


void RHIVulkanImageResource::Cleanup(RHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context->GetImpl());
	vkDestroyImage(VulkanContext->Device, Image, nullptr);
	vkDestroyImageView(VulkanContext->Device, DescriptorInfo.imageView, nullptr);
	vkFreeMemory(VulkanContext->Device, DeviceMemory, nullptr);
	if (bHasSampler)
	{
		vkDestroySampler(VulkanContext->Device, DescriptorInfo.sampler, nullptr);
	}
}

void RHIVulkanImageResource::Resize(RHIContext* Context, uint32_t Height, uint32_t Width) {

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