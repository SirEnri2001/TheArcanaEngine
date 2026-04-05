// RHI.h - RHI (Rendering Hardware Interface) Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
// @description RHI abstraction for different APIs 


#pragma once
#include "RHI.h"

#ifdef RHI_IMPLEMENT
#define RHI_API __declspec(dllexport)
#else
#ifdef RHI_INCLUDE
#define RHI_API __declspec(dllimport)
#else
#error Please specify API linkage before include this file
#define RHI_API
#endif
#endif

#include <cstdint>
#include <memory>
#include <vector>

class IRHICommandBuffer;
class IRHIContext;
class IRHIFrameBuffer;
class IRHIImageResource;
class IRHIImGUI;
class IRHIPipelineFactory;
class IRHIPipelineObject;
class IRHIPlatformSupport;
class IRHIRenderPass;
class IRHISwapchain;
class IRHIBuffer;

/** Pixel format used in all RHI implementations.
 * Every RHI implementation should always write its own convert function.
 */
enum RHIFormat
{
    //UNKNOWN, /**< format unknown, usually used in swapchain creation */
	R8G8B8A8_SRGB, /**< float4 in hlsl, used in color rendertargets */
    B8G8R8A8_SRGB, /**< float4 in hlsl, used in color rendertargets */
	R8G8B8A8_UNORM, /**< float4 in hlsl, used in color rendertargets */
    R32G32B32A32_SFLOAT, /**< float4 */
    R32G32B32_SFLOAT, /**< float3 */
    R32G32_SFLOAT, /**< float2, uv coords */
    D32_SFLOAT /**< float, depth value */
};

//enum class RHIImageUsageBits : uint32_t
//{
//    GENERAL = 0x0001u,  /**< General texture usage */
//    COLOR_RT = 0x0002u, /**< Color rendertarget, created with IRHIImageResource::InitializeRenderTarget */
//    DEPTH_RT = 0x0004u, /**< Depth rendertarget, created with IRHIImageResource::InitializeRenderTarget */
//    COLOR_PRESENT_RT = 0x0008u, /**< Color rendertarget, created with IRHIImageResource::InitializeRenderTarget, specified for present on surface */
//    DEPTH_PRESENT_RT = 0x0010u,  /**< Depth rendertarget, created with IRHIImageResource::InitializeRenderTarget, specified for present on surface */
//    STORAGE = 0x0020u, /**< Storage with read and write access in compute shader */
//    SAMPLER = 0x0040u,
//    TRANSFER_DST = 0x0080u,
//};
//
//inline RHIImageUsageBits operator&(RHIImageUsageBits Mask1, RHIImageUsageBits Mask2)
//{
//    return static_cast<RHIImageUsageBits>(static_cast<uint32_t>(Mask1) & static_cast<uint32_t>(Mask2));
//}

enum class RHIResourceState : uint32_t {
    UNDEFINED = 0,

    // Shader
    SHADER_READ = 1 << 0,
    SHADER_WRITE = 1 << 1,
    SHADER_READWRITE = 1 << 2,

    // Attachments
    COLOR_ATTACHMENT = 1 << 3,
    DEPTH_ATTACHMENT = 1 << 4,

    // Transfer
    COPY_SRC = 1 << 5,
    COPY_DST = 1 << 6,

    // Present
    PRESENTABLE = 1 << 7,

    // Buffers
    BUFFER_UNIFORM = 1 << 8,
    BUFFER_SHADER_STORAGE = 1 << 9,
    BUFFER_VERTEX = 1 << 10,
    BUFFER_INDEX = 1 << 11,
};

inline RHIResourceState operator&(RHIResourceState Mask1, RHIResourceState Mask2)
{
    return static_cast<RHIResourceState>(static_cast<uint32_t>(Mask1) & static_cast<uint32_t>(Mask2));
}

inline RHIResourceState operator|(RHIResourceState Mask1, RHIResourceState Mask2)
{
    return static_cast<RHIResourceState>(static_cast<uint32_t>(Mask1) | static_cast<uint32_t>(Mask2));
}

inline bool HasAnyFlags(RHIResourceState Mask)
{
    return static_cast<uint32_t>(Mask) != 0;
}

inline bool HasFlag(RHIResourceState Mask, RHIResourceState Flag)
{
    return static_cast<uint32_t>(Mask & Flag) != 0;
}

enum ImageFlag
{
	
};

enum BufferType
{
    GENERAL, /**< General buffer */
    VERTEX, /**< Vertex buffer */
    INDEX /**< Index buffer */
};

enum DescriptorType
{
	UNIFORM,
    SAMPLER2D,
    IMAGE2D,
    STORAGE,
    STORAGE_READONLY,
};

struct ImageExtent3D
{
    uint32_t Width;
    uint32_t Height;
    uint32_t Depth;
};

enum class RHIBackend: uint32_t
{
	Vulkan,
    D3D12,
};


/**
 * IRHIPlatformSupport provides Graphics API environment context. 
 * This class serves as a helper class to create & destroy window manager and context, and should be referenced as few as possible in the pipeline creation process. 
 * May vary by different hardware and OS. Should exist only one instance for each application.
 * In the initialization process, often we mix up the API layer initialization and window initialization.
 * So Initialize() function should doing all works before we have any window. Then use InitializePhysicalDevice to finalize the process.
 * @see RHIVulkanPlatformSupport Implementation for Vulkan API
 */
class RHI_API IRHIPlatformSupport
{
    static std::vector<std::unique_ptr<IRHIPlatformSupport>> GInstances;
public:
    IRHIPlatformSupport();
    virtual ~IRHIPlatformSupport() {}
	/**
	 * Should be called first in application.
	 */
	virtual void Initialize() = 0;
	/**
	 * Should be called last in the graphical context.
	 */
	virtual void Cleanup() = 0;
    virtual std::unique_ptr<IRHIContext> CreateRHIContext() = 0;
    static IRHIPlatformSupport* Get(RHIBackend InBackend);
};

/** IRHIContext serves as a general RHI resource allocator.
 * It should only has necessary fields and handles to create rendering resource like Images and Buffers.
 */
class RHI_API IRHIContext
{
public:
    IRHIContext() {}
    virtual ~IRHIContext() {}
    virtual void Initialize(uint32_t WindowWidth, uint32_t WindowHeight) = 0;
    virtual void Cleanup() = 0;
    virtual void WaitDeviceIdle() = 0;
    virtual void ProcessFrameInput() = 0;
    virtual bool IsWindowAlive() = 0;
    virtual std::unique_ptr<IRHICommandBuffer     > CreateRHICommandBuffer      () = 0;
    virtual std::unique_ptr<IRHIFrameBuffer       > CreateRHIFrameBuffer        () = 0;
    virtual std::unique_ptr<IRHIImageResource     > CreateRHIImageResource      () = 0;
    virtual std::unique_ptr<IRHIImGUI             > CreateRHIImGUI              () = 0;
    virtual std::unique_ptr<IRHIPipelineFactory   > CreateRHIPipelineFactory    () = 0;
    virtual std::unique_ptr<IRHIPipelineObject    > CreateRHIPipelineObject     () = 0;
    virtual std::unique_ptr<IRHIRenderPass        > CreateRHIRenderPass         () = 0;
    virtual std::unique_ptr<IRHISwapchain         > CreateRHISwapchain          () = 0;
    virtual std::unique_ptr<IRHIBuffer           > CreateRHIBuffer            () = 0;
};

/**
 * IRHIImageResource stores textures and rendertargets. 
 */
class RHI_API IRHIImageResource
{
public:
    IRHIImageResource() {}
    virtual ~IRHIImageResource() {}

    virtual void Initialize(IRHIContext* Context, uint32_t Height, uint32_t Width, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t MipLevel = -1) = 0;
    virtual void Initialize(IRHIContext* Context, ImageExtent3D RTExtent, RHIFormat InFormat, RHIResourceState InUsageMask, int32_t MipLevel = -1)  = 0;
    /**
     * Copy data to texture
     * @param CommandBuffer Command buffer to record copy commands
     * @param Context 
     * @param Data 
     * @param Stride 
     */
    virtual void CopyToTexture(IRHICommandBuffer* CommandBuffer, IRHIContext* Context, void* Data, uint32_t Stride) = 0;
    virtual void Cleanup(IRHIContext* Context) = 0;
    virtual void Resize(IRHIContext* Context, uint32_t Height, uint32_t Width) = 0;
    virtual void Transition(IRHICommandBuffer* CommandBuffer, RHIResourceState InState) = 0;
};

/**
 * IRHIBuffer stores uniform buffers.
 */
class RHI_API IRHIBuffer
{
public:
    IRHIBuffer() {}
    virtual ~IRHIBuffer() {}
    virtual void Initialize(IRHIContext* Context, uint32_t BufferSize, RHIResourceState InState) = 0;
    virtual void Initialize(IRHIContext* Context, uint32_t ElementSize, uint32_t Elements, RHIResourceState InState) = 0;
    virtual void CopyToBuffer(IRHIContext* Context, void* data, uint32_t TotalBytes) = 0;
    virtual void Cleanup(IRHIContext* Context) = 0;
};

/**
 * IRHIRenderPass stores framebuffers, and manages rendertargets.
 */
class RHI_API IRHIRenderPass
{
public:
    IRHIRenderPass() {}
    virtual ~IRHIRenderPass() {}
	virtual void Initialize(IRHIContext* Context, std::vector<RHIFormat> ColorRTFormats, bool hasDepth = true, RHIFormat DepthFormat = RHIFormat::D32_SFLOAT) = 0;
    virtual void Cleanup(IRHIContext* Context) = 0;
    virtual void BeginRenderPass(IRHICommandBuffer* CommandBuffer, IRHIFrameBuffer* Framebuffer) = 0;
    virtual void EndRenderPass(IRHICommandBuffer* CommandBuffer) = 0;
};

class RHI_API IRHIFrameBuffer
{
public:
    IRHIFrameBuffer() {}
    IRHIFrameBuffer(const IRHIFrameBuffer&) = delete;
    virtual ~IRHIFrameBuffer() {}
    virtual void Initialize(IRHIContext* Context, IRHIRenderPass* RenderPass,
        std::vector<IRHIImageResource*> ColorRTs, IRHIImageResource* DepthRT) = 0;
    virtual void Cleanup(IRHIContext* Context) = 0;
};

/**
 * IRHIPipelineFactory defines shader pipeline and vertex buffer descriptions. 
 */
class RHI_API IRHIPipelineFactory
{
public:
    IRHIPipelineFactory() {}
    virtual ~IRHIPipelineFactory() {}
    /**
     * 
     * @param BindingIndex Always use AddBinding add this binding index
     * @param Location defined in glsl: layout(location = xxx)
     * @param Format size of this vertex attribute
     * @param Offset use offset() macro
     */
    virtual void AddBufferLayout(uint32_t BindingIndex, uint32_t Location, RHIFormat Format, uint32_t Offset) = 0;
    /**
     * 
     * @param BindingIndex BindingIndex for buffer. Not necessarily differs from binding index of uniforms' and image samplers', but should be the same as in RHIGraphicsDispatcher::BindVertexBuffer / BindIndexBuffer
     * @param Stride 
     */
    virtual void AddBufferBinding(uint32_t BindingIndex, uint32_t Stride) = 0;
    virtual void RemoveAllBufferBindings() = 0;
    virtual void SetUniformBinding(uint32_t Binding) = 0;
    virtual void SetStorageBufferBinding(uint32_t Binding) = 0;
    virtual void SetImageSamplerBinding(uint32_t Binding) = 0;
    virtual void SetDescriptorBinding(uint32_t BindingIndex, DescriptorType BindingDescriptorType) = 0;
    virtual void RemoveAllGlobalBindings() = 0;
    virtual void SetShaders(const std::vector<char>& VertShader, const std::vector<char>& FragShader) = 0;
    virtual void SetComputeShaders(const std::vector<char>& ComputeShader) = 0;
    virtual void InitializePipelineObject(IRHIPipelineObject* OutPipelineObject, IRHIContext* Context, IRHIRenderPass* RenderPassResource) = 0;
    virtual void InitializeComputePipelineObject(IRHIPipelineObject* OutComputePipelineObject, IRHIContext* Context) = 0;
    virtual void Cleanup(IRHIContext* Context) = 0;
};

/**
 * IRHIPipelineFactory defines shader pipeline and vertex buffer descriptions. 
 */
class RHI_API IRHIPipelineObject
{
public:
    IRHIPipelineObject() {}
    virtual ~IRHIPipelineObject() {}
    virtual void SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, IRHIImageResource* ImageResource) = 0;
    virtual void SetBindingResource(uint32_t BindingIndex, DescriptorType BindingDescriptorType, IRHIBuffer* Uniform) = 0;
    virtual void SetUniform(IRHIBuffer* Uniform, uint32_t Binding) = 0;
    virtual void SetStorageBuffer(IRHIBuffer* StorageBuffer, uint32_t Binding) = 0;
    virtual void SetImageSampler(IRHIImageResource* ImageResource, uint32_t Binding) = 0;
    virtual void BindVertexBuffer(IRHIBuffer* Buffer, uint32_t Offset, uint32_t BindingIndex) = 0;
    virtual void BindIndexBuffer(IRHIBuffer* Buffer, uint32_t Offset) = 0;
    virtual void Cleanup(IRHIContext* Context) = 0;
    virtual void CopyDescriptors(IRHIContext* Context) = 0;

    virtual void Draw(IRHICommandBuffer* CommandBuffer, uint32_t IndexCount, uint32_t IndexOffset, uint32_t InstanceCount) = 0;
    virtual void Dispatch(IRHICommandBuffer* CommandBuffer, uint32_t ThreadGroupX, uint32_t ThreadGroupY, uint32_t ThreadGroupZ) = 0;
};

/**
 * IRHICommandBuffer stores command buffers for recording rendering commands.
 */
class RHI_API IRHICommandBuffer
{
public:
    IRHICommandBuffer() {}
    IRHICommandBuffer(const IRHICommandBuffer&) = delete;
    virtual ~IRHICommandBuffer() {}
    virtual void Initialize(IRHIContext* Context) = 0;
    virtual void Cleanup(IRHIContext* Context) = 0;
    virtual void BeginCommandBuffer() = 0;
    virtual void EndCommandBuffer() = 0;
    virtual void ResetCommandBuffer() = 0;
};

class RHI_API IRHISwapchain {
public:
    IRHISwapchain() {}
    IRHISwapchain(const IRHISwapchain&) = delete;
    virtual ~IRHISwapchain() {}
    virtual void Initialize(IRHIContext* Context, RHIFormat InSwapchainFormat) = 0;
    virtual void Cleanup(IRHIContext* Context) = 0;
    virtual void AcquireFrame(IRHIContext* Context, IRHIFrameBuffer*& OutFrameBuffer, IRHIRenderPass* InRenderPass) = 0;
    virtual void PresentFrameAndRelease(IRHIContext* Context, IRHICommandBuffer* CommandBuffer) = 0;
    virtual ImageExtent3D GetFrameSize() = 0;
    virtual RHIFormat GetFrameFormat() = 0;
};

struct ImGuiSharedGlobals;

/**
 * IRHIImGUI manages ImGUI objects with the rest of RHI interfaces.
 */
class RHI_API IRHIImGUI
{
public:
    IRHIImGUI() {}
    IRHIImGUI(const IRHIImGUI&) = delete;
    virtual ~IRHIImGUI() {}
    virtual void Initialize(IRHIContext* Context, IRHISwapchain* Swapchain, IRHIRenderPass* PresentRenderPass) = 0;
    /**
     * Executes drawcalls for ImGUI.
     * @param CommandBuffer Command buffer to record ImGUI draw commands
     */
    virtual void DispatchImGUI(IRHICommandBuffer* CommandBuffer) = 0;
    /**
     * UI components are defined here. 
     */
    virtual void UpdateUI(void (*pFuncDrawUI)(ImGuiSharedGlobals* context)) = 0;
	virtual void Cleanup() = 0;
};
