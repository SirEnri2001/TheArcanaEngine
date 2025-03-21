// RHI.cpp - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
#define RHI_IMPLEMENT
#include "RHI.h"

#include "Vulkan/RHIVulkan.h"

RHIImplementationSelection GRHIImplementationSelection = RHIImplement_Vulkan;

// RHIPlatformSupport implementation
RHIPlatformSupport::RHIPlatformSupport()
{
	pImpl = std::make_unique<RHIVulkanPlatformSupport>();
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

void RHIPlatformSupport::InitializePhysicalDevice(RHIWindowManager* WindowManager)
{
	pImpl->InitializePhysicalDevice(WindowManager);
}

// RHIContext implementation
RHIContext::RHIContext()
{
	pImpl = std::make_unique<RHIVulkanContext>();
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
	pImpl = std::make_unique<RHIVulkanWindowManager>();
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
	pImpl = std::make_unique<RHIVulkanImageResource>();
}

RHIImageResource::~RHIImageResource()
{
}

void RHIImageResource::Initialize(RHIContext* Context, const char* ImageFileName, RHIFormat InFormat, uint32_t MipLevel)
{
	pImpl->Initialize(Context, ImageFileName, InFormat, MipLevel);
}

void RHIImageResource::InitializeRenderTarget(RHIContext* Context, RHIWindowManager* WindowManager, ImageExtent3D RTExtent, ImageUsage InUsage, uint32_t MultiSamplesCount)
{
	pImpl->InitializeRenderTarget(Context, WindowManager, RTExtent, InUsage, MultiSamplesCount);
}

void RHIImageResource::Cleanup(RHIContext* Context)
{
	pImpl->Cleanup(Context);
}

// RHIBufferResource implementation
RHIBufferResource::RHIBufferResource()
{
	pImpl = std::make_unique<RHIVulkanBufferResource>();
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
	pImpl = std::make_unique<RHIVulkanUniform>();
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
	pImpl = std::make_unique<RHIVulkanRenderPass>();
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
	pImpl = std::make_unique<RHIVulkanPresentPass>();
}

RHIPresentPass::~RHIPresentPass()
{
}

void RHIPresentPass::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, uint32_t MSAASamples, RHIImageResource* ColorRT, RHIImageResource* DepthRT)
{
	pImpl->Initialize(Context, WindowManager, MSAASamples, ColorRT, DepthRT);
}

void RHIPresentPass::CreateSwapchainFramebuffer(RHIContext* Context, RHIWindowManager* WindowManager)
{
	pImpl->CreateSwapchainFramebuffer(Context, WindowManager);
}

void RHIPresentPass::CleanupSwapchainFramebuffer(RHIContext* Context)
{
	pImpl->CleanupSwapchainFramebuffer(Context);
}

void RHIPresentPass::Cleanup(RHIContext* Context)
{
	pImpl->Cleanup(Context);
}

void RHIPresentPass::OnWindowResize(RHIContext* Context, RHIWindowManager* WindowManager)
{
	pImpl->OnWindowResize(Context, WindowManager);
}


// RHIPipeline implementation
RHIPipeline::RHIPipeline()
{
	pImpl = std::make_unique<RHIVulkanPipeline>();
}

RHIPipeline::~RHIPipeline()
{
}

void RHIPipeline::AddLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset)
{
	pImpl->AddLayout(BindingIndex, Location, Format, Offset);
}

void RHIPipeline::AddBinding(uint32_t BindingIndex, uint32_t Stride)
{
	pImpl->AddBinding(BindingIndex, Stride);
}

void RHIPipeline::SetUniformBinding(RHIUniform* Uniform, uint32_t Binding)
{
	pImpl->SetUniformBinding(Uniform, Binding);
}

void RHIPipeline::SetImageSamplerBinding(RHIImageResource* ImageResource, uint32_t Binding)
{
	pImpl->SetImageSamplerBinding(ImageResource, Binding);
}

void RHIPipeline::SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader)
{
	pImpl->SetShaders(VertShader, FragShader);
}

void RHIPipeline::Initialize(RHIContext* Context, RHIRenderPass* RenderPassResource)
{
	pImpl->Initialize(Context, RenderPassResource);
}

void RHIPipeline::Initialize(RHIContext* Context, RHIPresentPass* PresentPass)
{
	pImpl->Initialize(Context, PresentPass);
}

void RHIPipeline::Cleanup(RHIContext* Context)
{
	pImpl->Cleanup(Context);
}

// RHIImGUI implementation
RHIImGUI::RHIImGUI()
{
	pImpl = std::make_unique<RHIVulkanImGUI>();
}

RHIImGUI::~RHIImGUI()
{
}

void RHIImGUI::Initialize(RHIContext* Context, RHIWindowManager* WindowManager, RHIPresentPass* PresentPass)
{
	pImpl->Initialize(Context, WindowManager, PresentPass);
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
	pImpl = std::make_unique<RHIVulkanGraphicDispatcher>();
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

void RHIGraphicsDispatcher::Dispatch(RHIWindowManager* WindowManager, RHIPipeline* Pipeline, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount)
{
	pImpl->Dispatch(WindowManager, Pipeline, IndexCount, IndexOffset, InstanceCount);
}

void RHIGraphicsDispatcher::BeginRenderPass(RHIContext* Context, RHIRenderPass* RenderPass)
{
	pImpl->BeginRenderPass(Context, RenderPass);
}


void RHIGraphicsDispatcher::BeginPresentPass(RHIContext* Context, RHIWindowManager* WindowManager, RHIPresentPass* PresentPassResource)
{
	pImpl->BeginPresentPass(Context, WindowManager, PresentPassResource);
}

void RHIGraphicsDispatcher::EndRenderPass(RHIRenderPass* RenderPass)
{
	pImpl->EndRenderPass(RenderPass);
}

void RHIGraphicsDispatcher::EndPresentPassAndSubmit(RHIContext* Context, RHIWindowManager* WindowManager)
{
	pImpl->EndPresentPassAndSubmit(Context, WindowManager);
}

void RHIGraphicsDispatcher::BeginFrame()
{
	pImpl->BeginFrame();
}


void RHIGraphicsDispatcher::WaitForGPUIdle(RHIContext* Context)
{
	pImpl->WaitForGPUIdle(Context);
}
