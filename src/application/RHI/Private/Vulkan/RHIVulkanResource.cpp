#include <cmath>

#define RHI_IMPLEMENT
#include "RHIVulkan.h"
#include "RHIVulkanImpl.h"


void RHIVulkanImageResource::Initialize(IRHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t InMipLevel)
{
	Initialize(Context, ImageExtent3D{ Width, Height, 1 }, InFormat, InUsageMask, InMipLevel);
}


void RHIVulkanImageResource::Initialize(IRHIContext* Context, ImageExtent3D RTExtent, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t InMipLevel)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	bHasSampler = HasFlag(InUsageMask, RHIResourceState::SHADER_READ);
	//Usage = InUsage;
	InnerFormat = RHIVulkanPlatformSupport::GetVkFormat(InFormat);
	ImageExtent = VkExtent3D{RTExtent.Width, RTExtent.Height, RTExtent.Depth};

	if (InMipLevel == -1)
	{
		InMipLevel = static_cast<uint32_t>(std::floor(std::log2(std::max(ImageExtent.width, ImageExtent.height)))) + 1;
	}
	MipLevel = bHasSampler ? InMipLevel : 1;

	// Check if image format supports linear blitting
	VkFormatProperties formatProperties = VulkanContext->GetFormatProperties(InnerFormat);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		//throw std::runtime_error("texture image format does not support linear blitting!");
	}

	VkImageUsageFlags VkImageUsage = 0;
	if (HasFlag(InUsageMask, RHIResourceState::SHADER_READ))
	{
		VkImageUsage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		VkImageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; // used when creating Mipmap level subresources
		VkImageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	if (HasFlag(InUsageMask, RHIResourceState::SHADER_WRITE))
	{
		VkImageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
	if (HasFlag(InUsageMask, RHIResourceState::COPY_DST))
	{
		VkImageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	if (HasFlag(InUsageMask, RHIResourceState::COLOR_ATTACHMENT))
	{
		VkImageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if (HasFlag(InUsageMask, RHIResourceState::DEPTH_ATTACHMENT))
	{
		VkImageUsage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	VkImageAspectFlagBits VkImageAspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT;
	if (HasFlag(InUsageMask, RHIResourceState::DEPTH_ATTACHMENT))
	{
		VkImageAspectFlagBits = VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	DescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	Desc = RHIVulkanPlatformSupport::GetStateDesc(RHIResourceState::UNDEFINED);
	CreateImage(Image, VulkanContext->Device, ImageExtent, MipLevel, VK_SAMPLE_COUNT_1_BIT, InnerFormat, VK_IMAGE_TILING_OPTIMAL, VkImageUsage);
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(VulkanContext->Device, Image, &memRequirements);
	CreateDeviceMemory(DeviceMemory, VulkanContext->Device, memRequirements.size, VulkanContext->GetMemoryType(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
	vkBindImageMemory(VulkanContext->Device, Image, DeviceMemory, 0);
	CreateImageView(DescriptorInfo.imageView, Image, VulkanContext->Device, InnerFormat, VkImageAspectFlagBits, 1);
	if (bHasSampler)
	{
		CreateSampler(DescriptorInfo.sampler, VulkanContext->Device, VulkanContext->CurrentPhysicalDevice.PDProperties.limits.maxSamplerAnisotropy);
		if (MipLevel > 1) {
			RHIVulkanCommandBuffer Buffer;
			Buffer.Initialize(Context);
			BeginCommandBufferOneTimeSubmit(Buffer.CommandBuffer);
			CreateMipmapForImage(Buffer.CommandBuffer, Image, ImageExtent.width, ImageExtent.height, MipLevel);
			EndCommandBufferOneTimeSubmit(Buffer.CommandBuffer, VulkanContext->GraphicsQueue);
		}
	}
}

VkDescriptorImageInfo& RHIVulkanImageResource::GetDescriptorImageInfo() {
	return DescriptorInfo;
}

 void RHIVulkanImageResource::CopyToTexture(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, void* Data, uint32_t Stride)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	auto* VulkanCommandBuffer = static_cast<RHIVulkanCommandBuffer*>(CommandBuffer);
	VkCommandBuffer VkCmdBuf = VulkanCommandBuffer->CommandBuffer;
	
	VkDeviceSize imageSize = ImageExtent.width * ImageExtent.height * Stride;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(VulkanContext->Device, Image, &memRequirements);
	{
		CreateBuffer(stagingBuffer, VulkanContext->Device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	    vkGetBufferMemoryRequirements(VulkanContext->Device, stagingBuffer, &memRequirements);
		CreateDeviceMemory(stagingBufferMemory, VulkanContext->Device, memRequirements.size, VulkanContext->GetMemoryType(memRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
		vkBindBufferMemory(VulkanContext->Device, stagingBuffer, stagingBufferMemory, 0);

		void* DstData;
		vkMapMemory(VulkanContext->Device, stagingBufferMemory, 0, imageSize, 0, &DstData);
		memcpy(DstData, Data, static_cast<size_t>(imageSize));
		vkUnmapMemory(VulkanContext->Device, stagingBufferMemory);
	}

	RHIVulkanCommandBuffer Buffer;
	Buffer.Initialize(Context);

	BeginCommandBufferOneTimeSubmit(Buffer.CommandBuffer);
	Transition(&Buffer, RHIResourceState::COPY_DST);
	CopyBufferToImage(stagingBuffer, Image, Buffer.CommandBuffer, ImageExtent.width, ImageExtent.height);
	CreateMipmapForImage(Buffer.CommandBuffer, Image, ImageExtent.width, ImageExtent.height, MipLevel);
	Transition(&Buffer, RHIResourceState::SHADER_READ);
	EndCommandBufferOneTimeSubmit(Buffer.CommandBuffer, VulkanContext->GraphicsQueue);

	vkDestroyBuffer(VulkanContext->Device, stagingBuffer, nullptr);
	vkFreeMemory(VulkanContext->Device, stagingBufferMemory, nullptr);
}

void RHIVulkanImageResource::CopyFromTexture(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, void* OutData, uint32_t Stride)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	
	VkDeviceSize imageSize = ImageExtent.width * ImageExtent.height * Stride;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	VkMemoryRequirements memRequirements;
	
	CreateBuffer(stagingBuffer, VulkanContext->Device, imageSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	vkGetBufferMemoryRequirements(VulkanContext->Device, stagingBuffer, &memRequirements);
	CreateDeviceMemory(stagingBufferMemory, VulkanContext->Device, memRequirements.size, VulkanContext->GetMemoryType(memRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	vkBindBufferMemory(VulkanContext->Device, stagingBuffer, stagingBufferMemory, 0);

	RHIVulkanCommandBuffer Buffer;
	Buffer.Initialize(Context);

	BeginCommandBufferOneTimeSubmit(Buffer.CommandBuffer);
	
	VkImageStateDesc OldDesc = Desc;
	Transition(&Buffer, RHIResourceState::COPY_SRC);
	CopyImageToBuffer(Image, stagingBuffer, Buffer.CommandBuffer, ImageExtent.width, ImageExtent.height);
	
	// Transition back to old state
	TransitionImageLayout(Image, Buffer.CommandBuffer, Desc.ImageLayout, OldDesc.ImageLayout, Desc.Access, OldDesc.Access, 
		Desc.PipelineStage, OldDesc.PipelineStage, MipLevel);
	Desc = OldDesc;
	DescriptorInfo.imageLayout = Desc.ImageLayout;

	EndCommandBufferOneTimeSubmit(Buffer.CommandBuffer, VulkanContext->GraphicsQueue);

	void* SrcData;
	vkMapMemory(VulkanContext->Device, stagingBufferMemory, 0, imageSize, 0, &SrcData);
	memcpy(OutData, SrcData, static_cast<size_t>(imageSize));
	vkUnmapMemory(VulkanContext->Device, stagingBufferMemory);

	vkDestroyBuffer(VulkanContext->Device, stagingBuffer, nullptr);
	vkFreeMemory(VulkanContext->Device, stagingBufferMemory, nullptr);
}


void RHIVulkanImageResource::Cleanup(IRHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	vkDestroyImage(VulkanContext->Device, Image, nullptr);
	vkDestroyImageView(VulkanContext->Device, DescriptorInfo.imageView, nullptr);
	vkFreeMemory(VulkanContext->Device, DeviceMemory, nullptr);
	if (bHasSampler)
	{
		vkDestroySampler(VulkanContext->Device, DescriptorInfo.sampler, nullptr);
	}
}

void RHIVulkanImageResource::Resize(IRHIContext* Context, uint32_t Height, uint32_t Width) {

}

void RHIVulkanImageResource::Transition(IRHICommandBuffer* CommandBuffer, RHIResourceState InState)
{
	auto VkCmdBuf = static_cast<RHIVulkanCommandBuffer*>(CommandBuffer)->CommandBuffer;
	VkImageStateDesc NewDesc = RHIVulkanPlatformSupport::GetStateDesc(InState);
	TransitionImageLayout(Image, VkCmdBuf, Desc.ImageLayout, NewDesc.ImageLayout, Desc.Access, NewDesc.Access, 
		Desc.PipelineStage, NewDesc.PipelineStage, MipLevel);
	Desc = NewDesc;
	DescriptorInfo.imageLayout = Desc.ImageLayout;
}


void RHIVulkanBuffer::Initialize(IRHIContext* Context, uint32_t BufferSize, RHIResourceState InState)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	if (
		!HasFlag(InState, RHIResourceState::BUFFER_SHADER_STORAGE) 
		&& !HasFlag(InState, RHIResourceState::BUFFER_UNIFORM)
		&& !HasFlag(InState, RHIResourceState::BUFFER_VERTEX)
		&& !HasFlag(InState, RHIResourceState::BUFFER_INDEX))
	{
		throw std::runtime_error("Wrong buffer initial state!");
	}
	VkBufferUsageFlags BufferUsage = 0u;
	if (HasFlag(InState, RHIResourceState::BUFFER_SHADER_STORAGE))
	{
		BufferUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}
	if (HasFlag(InState, RHIResourceState::BUFFER_UNIFORM))
	{
		BufferUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}
	if (HasFlag(InState, RHIResourceState::BUFFER_VERTEX))
	{
		BufferUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}
	if (HasFlag(InState, RHIResourceState::BUFFER_INDEX))
	{
		BufferUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}
	CreateBuffer(Buffer, VulkanContext->Device, BufferSize, BufferUsage);
	VkMemoryRequirements MemRequirements;
	vkGetBufferMemoryRequirements(VulkanContext->Device, Buffer, &MemRequirements);
	CreateDeviceMemory(DeviceMemory, VulkanContext->Device, MemRequirements.size, 
		VulkanContext->GetMemoryType(MemRequirements, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
	vkBindBufferMemory(VulkanContext->Device, Buffer, DeviceMemory, 0);
	vkMapMemory(VulkanContext->Device, DeviceMemory, 0, BufferSize, 0, &MappedMemory);
	DescriptorBufferInfo = {};
	DescriptorBufferInfo.buffer = Buffer;
	DescriptorBufferInfo.offset = 0;
	DescriptorBufferInfo.range = BufferSize;
	Size = BufferSize;
}

void RHIVulkanBuffer::CopyToBuffer(IRHIContext* Context, void* data, uint32_t TotalBytes)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	memcpy(MappedMemory, data, TotalBytes);
}

void RHIVulkanBuffer::Cleanup(IRHIContext* Context)
{
	auto* VulkanContext = static_cast<RHIVulkanContext*>(Context);
	vkUnmapMemory(VulkanContext->Device, DeviceMemory);
	vkDestroyBuffer(VulkanContext->Device, Buffer, nullptr);
	vkFreeMemory(VulkanContext->Device, DeviceMemory, nullptr);
}