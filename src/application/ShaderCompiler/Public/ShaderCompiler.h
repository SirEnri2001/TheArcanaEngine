// ShaderCompiler.h - Shader Compiler Module - TheArcanaEngine Project
// Copyright (c) 2025 Xinghua Han - MIT License
// @description Functionalities for compiling shaders on the fly


#pragma once

#ifdef SHADERCOMPILER_IMPLEMENT
#define SHADERCOMPILER_API __declspec(dllexport)
#else
#ifdef SHADERCOMPILER_INCLUDE
#define SHADERCOMPILER_API __declspec(dllimport)
#else
#error Please specify API linkage before include this file
#define SHADERCOMPILER_API
#endif
#endif

#include <string>

class SHADERCOMPILER_API GLSLCompiler {
public:
	void Preprocess(const char* File);
	bool DirectCompile(const char* InFile, const char* OutFile, const char* Args);
	void SetWorkingDirectory(const char* Dir);
	void CompileVertexShader(const char* File);
	void CompileFragmentShader(const char* File);
};
