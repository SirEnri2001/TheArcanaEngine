#pragma once
#define RHI_INCLUDE
#define CORESCENE_INCLUDE

#if defined(PBR_RENDERER_IMPLEMENT)
#define PBR_RENDERER_API _declspec(dllexport)
#elif defined(PBR_RENDERER_INCLUDE)
    #define PBR_RENDERER_API _declspec(dllimport)
#else
    #error Please Specify API Linkage PBR_RENDERER
    #define PBR_RENDERER_API
#endif


#include <string>
#include <vector>
#include <map>
#include "RHI.h"
#include "CoreMath.inl"
#include "CoreScene.h"

class RendererContext;
