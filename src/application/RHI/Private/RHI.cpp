// RHI.cpp - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
#define RHI_IMPLEMENT
#include "RHI.h"

#include "Vulkan/RHIVulkan.h"
#include "RHID3D12.h"

RHIImplementationSelection GRHIImplementationSelection = RHIImplement_Vulkan;

// RHIPlatformSupport implementation
RHIPlatformSupport::RHIPlatformSupport()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanPlatformSupport>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12PlatformSupport>();
	}
}

RHIPlatformSupport::~RHIPlatformSupport()
{
}

RHIPlatformSupport* RHIPlatformSupport::GInstance = nullptr;

RHIPlatformSupport* RHIPlatformSupport::Get()
{
	if(GInstance==nullptr)
	{
		GInstance = new RHIPlatformSupport();
	}
	return GInstance;
}

void RHIPlatformSupport::Initialize()
{
	pImpl->Initialize();
}

void RHIPlatformSupport::Cleanup()
{
	pImpl->Cleanup();
}

// RHIContext implementation
RHIContext::RHIContext()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanContext>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12Context>();
	}
}

RHIContext::~RHIContext()
{
}

void RHIContext::Initialize(RHIPlatformSupport* PlatformSupport)
{
	pImpl->Initialize(PlatformSupport);
}

void RHIContext::Cleanup()
{
	pImpl->Cleanup();
}

void RHIContext::WaitDeviceIdle()
{
	pImpl->WaitDeviceIdle();
}

// RHIWindowManager implementation
RHIWindowManager::RHIWindowManager()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanWindowManager>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12WindowManager>();
	}
}

RHIWindowManager::~RHIWindowManager()
{
}

void RHIWindowManager::Initialize(RHIPlatformSupport* PlatformSupport, uint32_t WindowHeight, uint32_t WindowWidth)
{
	pImpl->Initialize(PlatformSupport, WindowHeight, WindowWidth);
}

void RHIWindowManager::Cleanup(RHIPlatformSupport* PlatformSupport)
{
	pImpl->Cleanup(PlatformSupport);
}

void RHIWindowManager::InitializeSwapchain(RHIContext* Context, RHIPlatformSupport* PlatformSupport)
{
	pImpl->InitializeSwapchain(Context, PlatformSupport);
}

void RHIWindowManager::CleanupSwapchain(RHIContext* Context)
{
	pImpl->CleanupSwapchain(Context);
}

void RHIWindowManager::RecreateSwapchain(RHIContext* Context)
{
	pImpl->RecreateSwapchain(Context);
}

void RHIWindowManager::AddScreenSizeTexture(RHIImageResource* ImageResource)
{
	pImpl->AddScreenSizeTexture(ImageResource);
}

void RHIWindowManager::RemoveScreenSizeTexture(RHIImageResource* ImageResource)
{
	pImpl->RemoveScreenSizeTexture(ImageResource);
}

void RHIWindowManager::InitializeRenderPassAsPresent(RHIRenderPass* OutRenderPass, RHIContext* Context)
{
	pImpl->InitializeRenderPassAsPresent(OutRenderPass, Context);
}


bool RHIWindowManager::IsAlive()
{
	return pImpl->IsAlive();
}


RHIImageResource* RHIWindowManager::GetColorRenderTarget() {
	return pImpl->GetColorRenderTarget();
}
RHIImageResource* RHIWindowManager::GetDepthRenderTarget() {
	return pImpl->GetDepthRenderTarget();
}

// RHIImageResource implementation
RHIImageResource::RHIImageResource()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanImageResource>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12ImageResource>();
	}
}

RHIImageResource::~RHIImageResource()
{
}

void RHIImageResource::Initialize(RHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, ImageUsage InUsage, int32_t MipLevel)
{
	pImpl->Initialize(Context, Height, Width, InFormat, InUsage, MipLevel);
}


void RHIImageResource::InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage, uint32_t MultiSamplesCount)
{
	pImpl->InitializeRenderTarget(Context, WindowManager, RTExtent, InUsage, MultiSamplesCount);
}

void RHIImageResource::Initialize(RHIContext* Context, ImageExtent3D RTExtent, RHIFormat InFormat, ImageUsage InUsage, int32_t MipLevel)
{
	pImpl->Initialize(Context, RTExtent, InFormat, InUsage, MipLevel);
}


void RHIImageResource::CopyToTexture(RHICommandBuffer* CommandBuffer, RHIContext* Context, void* Data, uint32_t Stride)
{
	pImpl->CopyToTexture(CommandBuffer, Context, Data, Stride);
}

void RHIImageResource::Cleanup(RHIContext* Context)
{
	pImpl->Cleanup(Context);
}

void RHIImageResource::Resize(RHIContext* Context, uint32_t Height, uint32_t Width)
{
	pImpl->Resize(Context, Height, Width);
}

// RHIBufferResource implementation
RHIBufferResource::RHIBufferResource()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanBufferResource>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12BufferResource>();
	}
}

RHIBufferResource::~RHIBufferResource()
{
}

void RHIBufferResource::Initialize(RHIContext* Context, uint32_t Stride, uint32_t ElementCounts, BufferType Type)
{
	pImpl->Initialize(Context, Stride, ElementCounts, Type);
}

void RHIBufferResource::CopyToBuffer(RHICommandBuffer* CommandBuffer, RHIContext* Context, void* data, uint32_t TotalBytes)
{
	pImpl->CopyToBuffer(CommandBuffer, Context, data, TotalBytes);
}

void RHIBufferResource::Cleanup(RHIContext* Context)
{
	pImpl->Cleanup(Context);
}

// RHIUniform implementation
RHIUniform::RHIUniform()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanUniform>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12Uniform>();
	}
}

RHIUniform::~RHIUniform()
{
}

void RHIUniform::Initialize(RHIContext* Context, uint32_t UniformStructSize)
{
	pImpl->Initialize(Context, UniformStructSize);
}

void RHIUniform::CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes)
{
	pImpl->CopyToBuffer(Context, data, TotalBytes);
}

void RHIUniform::Cleanup(RHIContext* Context)
{
	pImpl->Cleanup(Context);
}

// RHIRenderPass implementation
RHIRenderPass::RHIRenderPass()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanRenderPass>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12RenderPass>();
	}
}

RHIRenderPass::~RHIRenderPass()
{
}

void RHIRenderPass::Initialize(RHIContext* Context, std::vector<RHIFormat> ColorRTFormats, bool hasDepth, RHIFormat DepthFormat)
{
	pImpl->Initialize(Context, ColorRTFormats, hasDepth, DepthFormat);
}

void RHIRenderPass::Cleanup(RHIContext* Context)
{
	pImpl->Cleanup(Context);
}

// RHIPresentPass implementation
RHIPresentPass::RHIPresentPass()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanPresentPass>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12PresentPass>();
	}
	
}

RHIPresentPass::~RHIPresentPass()
{
}

void RHIPresentPass::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples, RHIImageResource* ColorRT, RHIImageResource* DepthRT)
{
	pImpl->Initialize(Context, WindowManager, MSAASamples, ColorRT, DepthRT);
}

void RHIPresentPass::Cleanup(RHIContext* Context)
{
	pImpl->Cleanup(Context);
}

void RHIPresentPass::OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager)
{
	pImpl->OnWindowResize(Context, WindowManager);
}


// RHIPipelineFactory implementation
RHIPipelineFactory::RHIPipelineFactory()
{
		if (GRHIImplementationSelection == RHIImplement_Vulkan) {
	pImpl = std::make_unique<RHIVulkanPipelineFactory>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
	pImpl = std::make_unique<RHID3D12PipelineFactory>();
	}
}

RHIPipelineFactory::~RHIPipelineFactory()
{
}

void RHIPipelineFactory::AddBufferLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset)
{
	pImpl->AddBufferLayout(BindingIndex, Location, Format, Offset);
}

void RHIPipelineFactory::AddBufferBinding(uint32_t BindingIndex, uint32_t Stride)
{
	pImpl->AddBufferBinding(BindingIndex, Stride);
}

void RHIPipelineFactory::RemoveAllBufferBindings()
{
	pImpl->RemoveAllBufferBindings();
}


void RHIPipelineFactory::SetUniformBinding(uint32_t Binding)
{
	pImpl->SetUniformBinding(Binding);
}

void RHIPipelineFactory::SetImageSamplerBinding(uint32_t Binding)
{
	pImpl->SetImageSamplerBinding(Binding);
}

void RHIPipelineFactory::RemoveAllGlobalBindings()
{
	pImpl->RemoveAllGlobalBindings();
}

void RHIPipelineFactory::SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader)
{
	pImpl->SetShaders(VertShader, FragShader);
}

void RHIPipelineFactory::InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIRenderPass* RenderPassResource)
{
	pImpl->InitializePipelineObject(OutPipelineObject, Context, RenderPassResource);
}

void RHIPipelineFactory::InitializePipelineObject(RHIPipelineObject* OutPipelineObject, RHIContext* Context, RHIPresentPass* PresentPass)
{
	pImpl->InitializePipelineObject(OutPipelineObject, Context, PresentPass);
}

void RHIPipelineFactory::Cleanup(RHIContext* Context)
{
	pImpl->Cleanup(Context);
}

// RHIImGUI implementation
RHIImGUI::RHIImGUI()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanImGUI>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12ImGUI>();
	}
}

RHIImGUI::~RHIImGUI()
{
}

void RHIImGUI::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHISwapchain* Swapchain, RHIRenderPass* PresentRenderPass)
{
	pImpl->Initialize(Context, WindowManager, Swapchain, PresentRenderPass);
}

void RHIImGUI::UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* ImGlobals))
{
	pImpl->UpdateUI(pFuncDrawUI);
}


void RHIImGUI::Cleanup()
{
	pImpl->Cleanup();
}

void RHIImGUI::DispatchImGUI(RHICommandBuffer* CommandBuffer, RHIGraphicsDispatcher* Dispatcher)
{
	pImpl->DispatchImGUI(CommandBuffer, Dispatcher);
}

// RHIGraphicsDispatcher implementation
RHIGraphicsDispatcher::RHIGraphicsDispatcher()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanGraphicDispatcher>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12GraphicsDispatcher>();
	}
}

RHIGraphicsDispatcher::~RHIGraphicsDispatcher()
{
}

void RHIGraphicsDispatcher::Initialize(RHIContext* Context)
{
	pImpl->Initialize(Context);
}

void RHIGraphicsDispatcher::Cleanup(RHIContext* Context, RHIWindowManager* WindowManager)
{
	pImpl->Cleanup(Context, WindowManager);
}

void RHIGraphicsDispatcher::BindVertexBuffer(RHIBufferResource* BufferResource, uint32_t Offset, uint32_t BindingIndex)
{
	pImpl->BindVertexBuffer(BufferResource, Offset, BindingIndex);
}

void RHIGraphicsDispatcher::BindIndexBuffer(RHIBufferResource* BufferResource, uint32_t Offset)
{
	pImpl->BindIndexBuffer(BufferResource, Offset);
}

void RHIGraphicsDispatcher::Draw(RHICommandBuffer* CommandBuffer, RHIPipelineObject* PipelineObject, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount)
{
	pImpl->Draw(CommandBuffer, PipelineObject, IndexCount, IndexOffset, InstanceCount);
}

void RHIGraphicsDispatcher::BeginRenderPass(RHICommandBuffer* CommandBuffer, RHIRenderPass* RenderPass, RHIFrameBuffer* Framebuffer)
{
	pImpl->BeginRenderPass(CommandBuffer, RenderPass, Framebuffer);
}


void RHIGraphicsDispatcher::EndRenderPass(RHICommandBuffer* CommandBuffer, RHIRenderPass* RenderPass)
{
	pImpl->EndRenderPass(CommandBuffer, RenderPass);
}

void RHIGraphicsDispatcher::BeginFrame(RHICommandBuffer* CommandBuffer, RHIContext* Context, RHISwapchain* Swapchain, RHIRenderPass* PresentRenderPass)
{
	pImpl->BeginFrame(CommandBuffer, Context, Swapchain, PresentRenderPass);
}

void RHIGraphicsDispatcher::EndFrameAndSubmit(RHICommandBuffer* CommandBuffer, RHIContext* Context, RHIWindowManager* WindowManager, RHIFrameBuffer* PresentFrameBuffer)
{
	pImpl->EndFrameAndSubmit(CommandBuffer, Context, WindowManager, PresentFrameBuffer);
}



void RHIGraphicsDispatcher::WaitForGPUIdle(RHIContext* Context)
{
	pImpl->WaitForGPUIdle(Context);
}

void RHIGraphicsDispatcher::TransitionImageAsRenderTarget(RHICommandBuffer* CommandBuffer, RHIImageResource* Image)
{
	pImpl->TransitionImageAsRenderTarget(CommandBuffer, Image);
}

void RHIGraphicsDispatcher::TransitionImageAsShaderRead(RHICommandBuffer* CommandBuffer, RHIImageResource* Image)
{
	pImpl->TransitionImageAsShaderRead(CommandBuffer, Image);
}


RHIPipelineObject::RHIPipelineObject()
{
		if (GRHIImplementationSelection == RHIImplement_Vulkan) {
    pImpl = std::make_unique<RHIVulkanPipelineObject>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
    pImpl = std::make_unique<RHID3D12PipelineObject>();
	}
}

RHIPipelineObject::~RHIPipelineObject()
{
}

void RHIPipelineObject::SetUniform(RHIUniform* Uniform, uint32_t Binding)
{
    pImpl->SetUniform(Uniform, Binding);
}

void RHIPipelineObject::SetImageSampler(RHIImageResource* ImageResource, uint32_t Binding)
{
    pImpl->SetImageSampler(ImageResource, Binding);
}

void RHIPipelineObject::Cleanup(RHIContext* Context)
{
    pImpl->Cleanup(Context);
}

RHIFrameBuffer::RHIFrameBuffer() {
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanFrameBuffer>();
	}
	else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12FrameBuffer>();
	}
}

RHIFrameBuffer::~RHIFrameBuffer() {

}

void RHIFrameBuffer::Initialize(RHIContext* Context, RHIRenderPass* RenderPass,
	std::vector<RHIImageResource*> ColorRTs, RHIImageResource* DepthRT) {
	pImpl->Initialize(Context, RenderPass, ColorRTs, DepthRT);
}

void RHIFrameBuffer::Cleanup(RHIContext* Context) {
	pImpl->Cleanup(Context);
}

RHISwapchain::RHISwapchain()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanSwapchain>();
	}
	else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12Swapchain>();
	}
}

RHISwapchain::~RHISwapchain()
{
	
}

void RHISwapchain::Initialize(RHIContext* Context, RHIWindowManager* WindowManager)
{
	pImpl->Initialize(Context, WindowManager);
}

void RHISwapchain::Cleanup(RHIContext* Context)
{
	pImpl->Cleanup(Context);
}

void RHISwapchain::AcquireFrame(RHIContext* Context, RHIFrameBuffer*& OutFrameBuffer, RHIRenderPass* InRenderPass)
{
	pImpl->AcquireFrame(Context, OutFrameBuffer, InRenderPass);
}

void RHISwapchain::PresentFrameAndRelease(RHIContext* Context, RHICommandBuffer* CommandBuffer, RHIGraphicsDispatcher* GDispatcher)
{
	pImpl->PresentFrameAndRelease(Context, CommandBuffer, GDispatcher);
}


ImageExtent3D RHISwapchain::GetFrameSize()
{
	return pImpl->GetFrameSize();
}

// RHICommandBuffer implementation
RHICommandBuffer::RHICommandBuffer()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanCommandBuffer>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		// TODO: Implement RHID3D12CommandBuffer
		// pImpl = std::make_unique<RHID3D12CommandBuffer>();
	}
}

RHICommandBuffer::~RHICommandBuffer()
{
	// pImpl will be automatically cleaned up by unique_ptr
}

void RHICommandBuffer::Initialize(RHIContext* Context)
{
	if (pImpl) {
		pImpl->Initialize(Context);
	}
}

void RHICommandBuffer::Cleanup(RHIContext* Context)
{
	if (pImpl) {
		pImpl->Cleanup(Context);
	}
}

// RHIComputeDispatcher implementation
RHIComputeDispatcher::RHIComputeDispatcher()
{
	if (GRHIImplementationSelection == RHIImplement_Vulkan) {
		pImpl = std::make_unique<RHIVulkanComputeDispatcher>();
	} else if (GRHIImplementationSelection == RHIImplement_D3D12) {
		pImpl = std::make_unique<RHID3D12ComputeDispatcher>();
	}
}

RHIComputeDispatcher::~RHIComputeDispatcher()
{
	// pImpl will be automatically cleaned up by unique_ptr
}

void RHIComputeDispatcher::Initialize(RHIContext* Context)
{
	if (pImpl) {
		pImpl->Initialize(Context);
	}
}

void RHIComputeDispatcher::Cleanup(RHIContext* Context)
{
	if (pImpl) {
		pImpl->Cleanup(Context);
	}
}

void RHIComputeDispatcher::Dispatch(RHICommandBuffer* CommandBuffer, RHIPipelineObject* PipelineObject, uint32_t ThreadGroupX, uint32_t ThreadGroupY, uint32_t ThreadGroupZ)
{
	if (pImpl) {
		pImpl->Dispatch(CommandBuffer, PipelineObject, ThreadGroupX, ThreadGroupY, ThreadGroupZ);
	}
}

void RHIComputeDispatcher::WaitForGPUIdle(RHIContext* Context)
{
	if (pImpl) {
		pImpl->WaitForGPUIdle(Context);
	}
}