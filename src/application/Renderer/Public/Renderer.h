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
public:
	std::unique_ptr<IRHIPipelineObject> PipelineObject;
	std::vector<ShaderFileType> VertexShaderSPIRV;
	std::vector<ShaderFileType> FragmentShaderSPIRV;
	std::vector<ShaderFileType> ComputeShaderSPIRV;
	std::string VS_Filename;
	std::string FS_Filename;
	std::string CS_Filename;
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
	 *
	 * Stores filenames, derives SPIRV output paths (<source>.spv), reads the compiled bytecode,
	 * creates PipelineFactory/PipelineObject, and calls SetShaders on the factory.
	 */
    virtual void InitializeAsGraphics(IRHIContext* Context, IRHIRenderPass& RenderPass,
		const std::string& InVS, const std::string& InFS);

	/**
	 * Initialize a compute pipeline (CS).
	 * @param Context  RHI context for resource creation
	 * @param InCS     Compute shader GLSL source file path (e.g. "./shaders/glsl/Test.comp")
	 *
	 * Stores filename, derives SPIRV output path (<source>.spv), creates PipelineFactory/PipelineObject.
	 * Call CompilePipeline to read the compiled bytecode and configure the pipeline.
	 */
	virtual void InitializeAsCompute(IRHIContext* Context, const std::string& InCS);

	virtual void SetAllShaderBindings(IRHIContext* Context) = 0;

	/** Compile shaders and set up the pipeline. Reads SPIRV files and configures the pipeline factory. */
	virtual bool Compile(IRHIContext* Context, std::optional<IRHIRenderPass*> RenderPass);

	/** Release all GPU resources held by this pipeline. */
	virtual void Destroy(IRHIContext* Context);
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
	virtual void CreateRenderer(uint32_t Height, uint32_t Width, RHIBackend Backend) = 0;

	/** Execute a single render frame. */
	virtual void Render(float4 ViewPos, RenderControl* control) = 0;

	/** Capture the current frame and save it to the given path. */
	virtual void CaptureFrame(const std::string& Path) = 0;
};