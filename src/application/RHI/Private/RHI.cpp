// RHI.cpp - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
#define RHI_IMPLEMENT
#include "RHI.h"

#include "Vulkan/RHIVulkan.h"
#include "RHID3D12.h"

// IRHIPlatformSupport implementation

std::vector<std::unique_ptr<IRHIPlatformSupport>> IRHIPlatformSupport::GInstances;


IRHIPlatformSupport::IRHIPlatformSupport()
{
}

IRHIPlatformSupport* IRHIPlatformSupport::Get(RHIBackend InBackend)
{
	uint32_t Index = static_cast<uint32_t>(InBackend);
	GInstances.resize(8);
	if (GInstances[Index])
	{
		return GInstances[Index].get();
	}
	switch (InBackend)
	{
	case RHIBackend::Vulkan:
		GInstances[Index] = std::make_unique<RHIVulkanPlatformSupport>();
		break;
	case RHIBackend::D3D12:
		GInstances[Index] = std::make_unique<RHID3D12PlatformSupport>();
		break;
	default:
		break;
	}
	return GInstances[Index].get();
}

void IRHIImGUI::DrawImGUI(std::function<void(void)> pFuncDrawUI)
{
	pFuncDrawUI();
}
