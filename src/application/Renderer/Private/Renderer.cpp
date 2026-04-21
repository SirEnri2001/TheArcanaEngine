#define RENDERER_IMPLEMENT
#include "Renderer.h"

#define SHADERCOMPILER_INCLUDE
#include "ShaderCompiler.h"

#include <algorithm>
#include "spirv_cross.hpp"
#include "spirv_glsl.hpp"

void IPipeline::InitializeAsGraphics(IRHIContext* Context, IRHIRenderPass& RenderPass,
	const std::string& InVS, const std::string& InFS, uint32_t InStride,
	const std::string& InVSEntry, const std::string& InFSEntry) {
	Type = EPipelineType::VS_FS;
	VS_Filename = InVS;
	FS_Filename = InFS;
	VS_EntryPoint = InVSEntry;
	FS_EntryPoint = InFSEntry;
	VertexBufferStride = InStride;
	PipelineFactory = Context->CreateRHIPipelineFactory();
	PipelineObject = Context->CreateRHIPipelineObject();
}

void IPipeline::InitializeAsCompute(IRHIContext* Context, const std::string& InCS, const std::string& InCSEntry) {
	Type = EPipelineType::CS;
	CS_Filename = InCS;
	CS_EntryPoint = InCSEntry;
	PipelineFactory = Context->CreateRHIPipelineFactory();
	PipelineObject = Context->CreateRHIPipelineObject();
}

bool IPipeline::Compile(IRHIContext* Context, std::optional<IRHIRenderPass*> RenderPass) {
	if (Type == EPipelineType::VS_FS && !RenderPass.has_value()) {
		throw std::runtime_error("Compile a graphics pipeline must provide renderpass");
	}

	GLSLCompiler Compiler;
	bool IsSucceeded = true;
	if (Type == EPipelineType::VS_FS) {
		IsSucceeded = IsSucceeded && Compiler.DirectCompile(
			std::string("./shaders/" + VS_Filename).c_str(),
			std::string("./shaderbytecode/" + VS_Filename + ".spv").c_str(), VS_EntryPoint.c_str()
		);
		IsSucceeded = IsSucceeded && Compiler.DirectCompile(
			std::string("./shaders/" + FS_Filename).c_str(),
			std::string("./shaderbytecode/" + FS_Filename + ".spv").c_str(), FS_EntryPoint.c_str()
		);
	}
	if (Type == EPipelineType::CS) {
		IsSucceeded = IsSucceeded && Compiler.DirectCompile(
			std::string("./shaders/" + CS_Filename).c_str(),
			std::string("./shaderbytecode/" + CS_Filename + ".spv").c_str(), CS_EntryPoint.c_str()
		);
	}
	if (IsSucceeded) {
		if (PipelineObject != nullptr) {
			PipelineObject->Cleanup(Context);
		}

		if (Type == EPipelineType::VS_FS) {
			VertexShaderSPIRV = readFile("./shaderbytecode/" + VS_Filename + ".spv");
			FragmentShaderSPIRV = readFile("./shaderbytecode/" + FS_Filename + ".spv");
			PipelineFactory->SetShaders(VertexShaderSPIRV, FragmentShaderSPIRV);
			SetAllShaderBindings(Context);
			PipelineFactory->InitializePipelineObject(PipelineObject.get(), Context, RenderPass.value());
		}
		else if (Type == EPipelineType::CS) {
			ComputeShaderSPIRV = readFile("./shaderbytecode/" + CS_Filename + ".spv");
			PipelineFactory->SetComputeShaders(ComputeShaderSPIRV);
			SetAllShaderBindings(Context);
			PipelineFactory->InitializeComputePipelineObject(PipelineObject.get(), Context);
		}
	}
	return IsSucceeded;
}

void AutoPipeline::SetAllShaderBindings(IRHIContext* Context) {
	if (!PipelineFactory) return;

	PipelineFactory->RemoveAllGlobalBindings();
	PipelineFactory->RemoveAllBufferBindings();

	if (Type == EPipelineType::VS_FS) {
		spirv_cross::Compiler VSCompiler(reinterpret_cast<const uint32_t*>(VertexShaderSPIRV.data()), VertexShaderSPIRV.size() / sizeof(uint32_t));
		spirv_cross::ShaderResources VSResources = VSCompiler.get_shader_resources();
		ReflectShaderBindings(VSCompiler, VSResources, IRHIPipelineFactory::EPipelineStages::VS_FS);
		ReflectVertexAttributes(VSCompiler, VSResources);

		spirv_cross::Compiler FSCompiler(reinterpret_cast<const uint32_t*>(FragmentShaderSPIRV.data()), FragmentShaderSPIRV.size() / sizeof(uint32_t));
		spirv_cross::ShaderResources FSResources = FSCompiler.get_shader_resources();
		ReflectShaderBindings(FSCompiler, FSResources, IRHIPipelineFactory::EPipelineStages::VS_FS);
	}
	else if (Type == EPipelineType::CS) {
		spirv_cross::Compiler CSCompiler(reinterpret_cast<const uint32_t*>(ComputeShaderSPIRV.data()), ComputeShaderSPIRV.size() / sizeof(uint32_t));
		spirv_cross::ShaderResources CSResources = CSCompiler.get_shader_resources();
		ReflectShaderBindings(CSCompiler, CSResources, IRHIPipelineFactory::EPipelineStages::CS);
	}
}

void AutoPipeline::ReflectShaderBindings(spirv_cross::Compiler& compiler, const spirv_cross::ShaderResources& resources, IRHIPipelineFactory::EPipelineStages stage) {
	auto AddBindings = [&](const spirv_cross::SmallVector<spirv_cross::Resource>& list, DescriptorType type) {
		for (auto& res : list) {
			uint32_t binding = compiler.get_decoration(res.id, spv::DecorationBinding);
			
			DescriptorType finalType = type;
			if (type == DescriptorType::STORAGE) {
				auto flags = compiler.get_buffer_block_flags(res.id);
				if (flags.get(spv::DecorationNonWritable)) {
					finalType = DescriptorType::STORAGE_READONLY;
				}
			}
			PipelineFactory->SetDescriptorBinding(binding, finalType, stage);
		}
	};

	AddBindings(resources.uniform_buffers, DescriptorType::UNIFORM);
	AddBindings(resources.storage_buffers, DescriptorType::STORAGE);
	AddBindings(resources.sampled_images, DescriptorType::SAMPLER2D);
	AddBindings(resources.storage_images, DescriptorType::IMAGE2D);
}

void AutoPipeline::ReflectVertexAttributes(spirv_cross::Compiler& compiler, const spirv_cross::ShaderResources& resources) {
	if (VertexBufferStride == 0) return;

	if (resources.stage_inputs.empty()) return;

	// Collect all vertex attributes
	struct VertexAttribute {
		uint32_t location;
		uint32_t binding;
		RHIFormat format;
		uint32_t offset;
	};
	std::vector<VertexAttribute> attributes;
	attributes.resize(resources.stage_inputs.size());

	constexpr uint32_t ALIGNMENT = 16;

	for (auto& input : resources.stage_inputs) {
		uint32_t location = compiler.get_decoration(input.id, spv::DecorationLocation);
		// only use 0 binding - one vertex buffer for now
		uint32_t binding = 0;//compiler.get_decoration(input.id, spv::DecorationBinding);
		
		auto& type = compiler.get_type(input.type_id);
		
		// Map SPIR-V type to RHIFormat
		RHIFormat format = RHIFormat::R32G32B32A32_SFLOAT;
		uint32_t vecSize = type.vecsize;
		
		switch (vecSize) {
		case 1: format = RHIFormat::R32_SFLOAT; break;
		case 2: format = RHIFormat::R32G32_SFLOAT; break;
		case 3: format = RHIFormat::R32G32B32_SFLOAT; break;
		case 4: format = RHIFormat::R32G32B32A32_SFLOAT; break;
		default: format = RHIFormat::R32G32B32A32_SFLOAT; break;
		}

		attributes[location] = {location, binding, format, 0};
	}

	if (attributes.empty()) return;
	uint32_t offset = 0;
	for (auto& attr : attributes) {
		PipelineFactory->AddBufferLayout(0, attr.location, attr.format, offset);
		offset += std::min((int)VertexBufferStride, 16);
	}

	// Add final binding
	PipelineFactory->AddBufferBinding(0, VertexBufferStride);
}

void IPipeline::Destroy(IRHIContext* Context) {
	if (PipelineObject) {
		PipelineObject->Cleanup(Context);
	}
	if (PipelineFactory) {
		PipelineFactory->Cleanup(Context);
	}
}
