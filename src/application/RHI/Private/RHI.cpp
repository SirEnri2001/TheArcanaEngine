// RHI.cpp - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
#define RHI_IMPLEMENT
#include "RHI.h"

#include "Vulkan/RHIVulkan.h"
#include "RHID3D12.h"

RHIImplementationSelection GRHIImplementationSelection = RHIImplement_Vulkan;

// IRHIPlatformSupport implementation

IRHIPlatformSupport* IRHIPlatformSupport::GInstance = nullptr;

IRHIPlatformSupport* IRHIPlatformSupport::Get()
{
	if (GInstance)
	{
		return GInstance;
	}
	GInstance = new RHID3D12PlatformSupport();
	return GInstance;
}
