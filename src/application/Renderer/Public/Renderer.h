#pragma once

#ifdef RENDERER_IMPLEMENT
#define RENDERER_API __declspec(dllexport)
#else
#ifdef RENDERER_INCLUDE
#define RENDERER_API __declspec(dllimport)
#else
#error Please specify API linkage before include this file
#define RENDERER_API
#endif
#endif

#include <string>
#include <stdexcept>
#include <fstream>
#include <optional>

#define RHI_INCLUDE
#include "RHI.h"

#include "CoreMath.h"
#include "CoreMath.inl"

#define SHADER_SOURCE_DIR(Filename) "./shaders/" + Filename
#define SHADER_SPIRV_DIR(Filename) "./shaderbytecode/" + Filename + ".spv"

struct RenderControl {
	bool Recompile = false;
	bool TestShaders = false;
	bool RenderingPaused = false;
	bool ShaderCompileHasError = false;
	float4x4 CameraTransformLocalToWorld;
	float4x4 CameraLookAt;
	float4x4 CameraPersp;
	bool isClick = false;
	bool bClear = false;
	int maxFrames = -1;
	int currentFrame = 0;
	bool enableGamma = true;
	float movingSpeed = 0.5;
	bool useSinglePass = true;

	// Pipeline selection
	std::vector<std::string> pipelines;
	int pipelineSelected = 0;

	// Path tracing parameters
	int totalIters = 1;
	int dispatchDepth = 4;
	float roughness = 0.3f;
	float prob_lambert = 0.5f;
	bool enableNEE = true;
};

typedef char ShaderFileType;

/** Utility: read a binary file into a vector<char>. */
inline std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("failed to open file: " + filename);
	}
	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);
	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();
	return buffer;
}

/**
 * IPipeline - Abstract base class for all rendering pipelines.
 * Encapsulates a PipelineFactory (for configuration) and a PipelineObject (the runtime pipeline).
 * InitializePipeline accepts shader source filenames, compiles them to SPIRV, loads the bytecode,
 * and creates the RHI pipeline factory/object. Subclasses override to add descriptor bindings etc.
 */
class IPipeline {
protected:
	std::unique_ptr<IRHIPipelineFactory> PipelineFactory;
	uint32_t VertexBufferStride = 0;
public:
	std::unique_ptr<IRHIPipelineObject> PipelineObject;
	std::vector<ShaderFileType> VertexShaderSPIRV;
	std::vector<ShaderFileType> FragmentShaderSPIRV;
	std::vector<ShaderFileType> ComputeShaderSPIRV;
	std::string VS_Filename;
	std::string FS_Filename;
	std::string CS_Filename;
	std::string VS_EntryPoint = "main";
	std::string FS_EntryPoint = "main";
	std::string CS_EntryPoint = "main";
	enum class EPipelineType {
		VS_FS,
		CS
	} Type;
    IPipeline() = default;
    IPipeline(const IPipeline& ) = delete;
    virtual ~IPipeline() {};

	/**
	 * Initialize a graphics pipeline (VS+FS).
	 * @param Context     RHI context for resource creation
	 * @param RenderPass  The render pass this pipeline binds to
	 * @param InVS        Vertex shader GLSL source file path   (e.g. "./shaders/glsl/PathTracer.vert")
	 * @param InFS        Fragment shader GLSL source file path (e.g. "./shaders/glsl/PathTracer.frag")
	 * @param InStride    Vertex buffer stride (sizeof(VertexType))
	 * @param InVSEntry   Vertex shader entry point (default: "main")
	 * @param InFSEntry   Fragment shader entry point (default: "main")
	 *
	 * Stores filenames, derives SPIRV output paths (<source>.spv), reads the compiled bytecode,
	 * creates PipelineFactory/PipelineObject, and calls SetShaders on the factory.
	 */
    virtual void InitializeAsGraphics(IRHIContext* Context, IRHIRenderPass& RenderPass,
		const std::string& InVS, const std::string& InFS, uint32_t InStride,
		const std::string& InVSEntry = "main", const std::string& InFSEntry = "main");

	/**
	 * Initialize a compute pipeline (CS).
	 * @param Context    RHI context for resource creation
	 * @param InCS       Compute shader GLSL source file path (e.g. "./shaders/glsl/Test.comp")
	 * @param InCSEntry  Compute shader entry point (default: "main")
	 *
	 * Stores filename, derives SPIRV output path (<source>.spv), creates PipelineFactory/PipelineObject.
	 * Call CompilePipeline to read the compiled bytecode and configure the pipeline.
	 */
	virtual void InitializeAsCompute(IRHIContext* Context, const std::string& InCS,
		const std::string& InCSEntry = "main");

	virtual void SetAllShaderBindings(IRHIContext* Context) = 0;

	/** Compile shaders and set up the pipeline. Reads SPIRV files and configures the pipeline factory. */
	virtual bool Compile(IRHIContext* Context, std::optional<IRHIRenderPass*> RenderPass);

	/** Release all GPU resources held by this pipeline. */
	virtual void Destroy(IRHIContext* Context);
};

namespace spirv_cross {
	class Compiler;
	class ShaderResources;
};

/**
 * AutoPipeline - A pipeline implementation that automatically reflects shader bindings using spirv-cross.
 * Automatically sets up descriptor bindings and vertex attributes from compiled SPIR-V shaders.
 */
class AutoPipeline : public IPipeline {
public:
	void SetAllShaderBindings(IRHIContext* Context) override;

private:
	void ReflectShaderBindings(spirv_cross::Compiler& compiler, const spirv_cross::ShaderResources& resources, IRHIPipelineFactory::EPipelineStages stage);
	void ReflectVertexAttributes(spirv_cross::Compiler& compiler, const spirv_cross::ShaderResources& resources);
};

/**
 * IRenderer - Abstract base class for all renderers.
 * Defines the interface for renderer lifecycle (create, render, shutdown)
 * and pipeline management (create, reload, release).
 */
class IRenderer {
public:
	virtual ~IRenderer() = default;

	// --- Renderer lifecycle interface ---

	/** Create and initialize the renderer with the given viewport dimensions. */
	virtual void CreateRenderer(uint32_t Height, uint32_t Width, RHIBackend Backend, bool bEnableValidation = true) = 0;

	/** Execute a single render frame. */
	virtual void Render(float4 ViewPos, RenderControl* control) = 0;

	/** Capture the current frame and save it to the given path. */
	virtual void CaptureFrame(const std::string& Path) = 0;
};