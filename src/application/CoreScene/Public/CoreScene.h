#pragma once

#ifdef CORESCENE_IMPLEMENT
#define CORESCENE_API __declspec(dllexport)
#else
#ifdef CORESCENE_INCLUDE
#define CORESCENE_API __declspec(dllimport)
#else
#error Please specify API linkage before include this file
#define CORESCENE_API
#endif
#endif

#include <string>
#include "CoreMath.inl"
#define COREGEOMETRY_INCLUDE
#include "CoreGeometry.h"

#include <memory>
#include <array>

using Vertex = TVertex<float3, float3, float2, float3, float3>;
using Mesh = TMesh<Vertex, uint32_t>;

struct CORESCENE_API Transform
{
    float3 RotationXYZ;
    float3 Translation;
    float3 Scale = float3(1.f,1.f,1.f);
    
};
float4x4 ToMatrix(const Transform& Trans);
float4x4 GetRotationMat(const Transform& Trans);

class CORESCENE_API Object
{
public:
    Mesh* MeshInstance = nullptr;
    Transform Local{};
    float4x4 GetLocalToWorld();
    std::vector<Object*> Children;
    Object* Parent;
    virtual ~Object();
};

class CORESCENE_API Scene : Object
{
public:
    static bool LoadSceneJson(Scene& OutScene, std::string JsonString);


    // Object & Static Mesh ownership allocation
    uint32_t OwnMeshSize = 0u;
    uint32_t OwnObjectSize = 0u;

    std::array<std::unique_ptr<Mesh>, 256> OwnMeshes;
    std::array<std::unique_ptr<Object>, 256> OwnObjects;
};

template<typename TransformType, typename MatrixType>
class IGeometryTransformable
{
public:
    virtual ~IGeometryTransformable();
    TransformType LocalTransform;
    virtual MatrixType GetLocalToWorldMat() = 0;
};

template<typename NodeType>
class INodeTraversable
{
public:
    virtual ~INodeTraversable();
	NodeType Parent;
    std::vector<NodeType> Children;
};

