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

uint32_t RHIWindowManager::GetWindowHeight()
{
	return pImpl->GetWindowHeight();
}

uint32_t RHIWindowManager::GetWindowWidth()
{
	return pImpl->GetWindowWidth();
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

void RHIImageResource::Initialize(RHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, int32_t MipLevel)
{
	pImpl->Initialize(Context, Height, Width, InFormat, MipLevel);
}


void RHIImageResource::InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage, uint32_t MultiSamplesCount)
{
	pImpl->InitializeRenderTarget(Context, WindowManager, RTExtent, InUsage, MultiSamplesCount);
}

void RHIImageResource::CopyToTexture(RHIContext* Context, void* Data, uint32_t Stride)
{
	pImpl->CopyToTexture(Context, Data, Stride);
}

void RHIImageResource::Cleanup(RHIContext* Context)
{
	pImpl->Cleanup(Context);
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

void RHIBufferResource::CopyToBuffer(RHIContext* Context, void* data, uint32_t TotalBytes)
{
	pImpl->CopyToBuffer(Context, data, TotalBytes);
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

void RHIRenderPass::Initialize(RHIContext* Context, uint32_t SizeX, uint32_t SizeY)
{
	pImpl->Initialize(Context, SizeX, SizeY);
}

void RHIRenderPass::Cleanup(RHIContext* Context)
{
	pImpl->Cleanup(Context);
}

void RHIRenderPass::AddColorRenderTarget(RHIImageResource* ColorRT)
{
	pImpl->AddColorRenderTarget(ColorRT);
}

void RHIRenderPass::SetDepthRenderTarget(RHIImageResource* DepthRT)
{
	pImpl->SetDepthRenderTarget(DepthRT);
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

void RHIImGUI::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* PresentRenderPass)
{
	pImpl->Initialize(Context, WindowManager, PresentRenderPass);
}

void RHIImGUI::UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* ImGlobals))
{
	pImpl->UpdateUI(pFuncDrawUI);
}


void RHIImGUI::Cleanup()
{
	pImpl->Cleanup();
}

void RHIImGUI::DispatchImGUI(RHIGraphicsDispatcher* Dispatcher)
{
	pImpl->DispatchImGUI(Dispatcher);
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

void RHIGraphicsDispatcher::Dispatch(RHIPipelineObject* PipelineObject, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount)
{
	pImpl->Dispatch(PipelineObject, IndexCount, IndexOffset, InstanceCount);
}

void RHIGraphicsDispatcher::BeginRenderPass(RHIRenderPass* RenderPass)
{
	pImpl->BeginRenderPass(RenderPass);
}


void RHIGraphicsDispatcher::EndRenderPass(RHIRenderPass* RenderPass)
{
	pImpl->EndRenderPass(RenderPass);
}

void RHIGraphicsDispatcher::BeginFrame(RHIContext* Context, RHIWindowManager* WindowManager, RHIRenderPass* PresentRenderPass)
{
	pImpl->BeginFrame(Context, WindowManager, PresentRenderPass);
}

void RHIGraphicsDispatcher::EndFrameAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager)
{
	pImpl->EndFrameAndSubmit(Context, WindowManager);
}



void RHIGraphicsDispatcher::WaitForGPUIdle(RHIContext* Context)
{
	pImpl->WaitForGPUIdle(Context);
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
