#define SHADERCOMPILER_IMPLEMENT
#include "ShaderCompiler.h"

#include <string>
#include <sstream>

void GLSLCompiler::Preprocess(const char* File) {

}

bool GLSLCompiler::DirectCompile(const char* InFile, const char* OutFile, const char* EntryPoint, const char* Args) {
	std::stringstream CommandLine;
	CommandLine << "glslc " << InFile << " -o " << OutFile << " -fentry-point=" << EntryPoint << " " << Args;
	int result = std::system(CommandLine.str().c_str());
	return result == 0;
}

void GLSLCompiler::SetWorkingDirectory(const char* Dir) {

}
void GLSLCompiler::CompileVertexShader(const char* File) {

}
void GLSLCompiler::CompileFragmentShader(const char* File) {

}