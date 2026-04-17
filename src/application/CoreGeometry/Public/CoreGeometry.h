#pragma once

#ifdef COREGEOMETRY_IMPLEMENT
#define COREGEOMETRY_API __declspec(dllexport)
#else
#ifdef COREGEOMETRY_INCLUDE
#define COREGEOMETRY_API __declspec(dllimport)
#else
#error Please specify API linkage before include this file
#define COREGEOMETRY_API
#endif
#endif
#include <string>
#include <vector>
#include <algorithm>
#include <array>
#include <map>
#include "tiny_obj_loader.h"

namespace CoreGeometryImpl
{
	bool COREGEOMETRY_API TinyObjLoadObj(tinyobj::attrib_t *attrib, std::vector<tinyobj::shape_t> *shapes,
             std::vector<tinyobj::material_t> *materials, std::string *err,
             const char *filename, const char *mtl_basedir = nullptr,
             bool triangulate = true);
}


template<typename TPos, typename ColorType, typename TexCoordType, typename TNor, typename TangentType>
struct TVertex {
    using PositionType = TPos;
    using NormalType = TNor;
    alignas(16) PositionType Position;
    alignas(16) ColorType Color;
    alignas(16) TexCoordType TexCoord;
    alignas(16) NormalType Normal;
    alignas(16) TangentType TangentTypeX;
    alignas(16) TangentType TangentTypeY;
};

template<typename TVertex, typename TIndex>
class TMesh
{
public:
    using VertexType = TVertex;
    using IndexType = TIndex;
    std::vector<VertexType> Vertices;
    std::vector<IndexType> Indices;
    std::string TexturePath;

    static TMesh<VertexType, IndexType> LoadObj(const std::string ObjFileName);
};


template<typename VertexType, typename IndexType>
struct BVHBox {
    alignas(16) typename VertexType::PositionType BoxMin;
    alignas(16) typename VertexType::PositionType BoxMax;
    int ChildIndex1;
    int ChildIndex2;
    IndexType TriangleIndexStart;
    IndexType TriangleIndexEnd;
};


// split the mesh at [0, SplitPos] and [SplitPos, MaxTriangleCount]
template<typename VertexType, typename IndexType>
int BuildBVHBox_Recursive(std::vector<BVHBox<VertexType, IndexType>>& BVH_Array,
    typename std::vector<std::array<VertexType, 3>>::iterator MeshStart,
    typename std::vector<std::array<VertexType, 3>>::iterator TIter_Start,
    typename std::vector<std::array<VertexType, 3>>::iterator TIter_End, size_t TrianglePerBox = 1)
{
    if (TIter_Start >= TIter_End) {
        return -1;
    }

    BVHBox<VertexType, IndexType> MeshBoundingBox;
    MeshBoundingBox.BoxMax = typename VertexType::PositionType(-std::numeric_limits<float>::max());
    MeshBoundingBox.BoxMin = typename VertexType::PositionType(std::numeric_limits<float>::max());

    for (auto TIter = TIter_Start; TIter < TIter_End; TIter++) {
        for (int i = 0; i < 3; i++) {
            auto& Vert = (*TIter)[i];
            for (int j = 0; j < 3; j++) {
                MeshBoundingBox.BoxMax[j] = std::max(Vert.Position[j], MeshBoundingBox.BoxMax[j]);
                MeshBoundingBox.BoxMin[j] = std::min(Vert.Position[j], MeshBoundingBox.BoxMin[j]);
            }
        }
    }

    MeshBoundingBox.TriangleIndexStart = (IndexType)(TIter_Start - MeshStart);
    MeshBoundingBox.TriangleIndexEnd = (IndexType)(TIter_End - MeshStart);

    if (TIter_End - TIter_Start <= TrianglePerBox) {
        MeshBoundingBox.ChildIndex1 = -1;
        MeshBoundingBox.ChildIndex2 = -1;
        int CurrentBoundingIndex = (int)BVH_Array.size();
        BVH_Array.push_back(MeshBoundingBox);
        return CurrentBoundingIndex;
    }

    auto BoxExtent = MeshBoundingBox.BoxMax - MeshBoundingBox.BoxMin;
    int SplitAxis = 0;
    if (BoxExtent[1] > BoxExtent[0] && BoxExtent[1] > BoxExtent[2]) SplitAxis = 1;
    else if (BoxExtent[2] > BoxExtent[0] && BoxExtent[2] > BoxExtent[1]) SplitAxis = 2;

    std::sort(TIter_Start, TIter_End, [SplitAxis](const std::array<VertexType, 3>& T1, const std::array<VertexType, 3>& T2) {
        float c1 = (T1[0].Position[SplitAxis] + T1[1].Position[SplitAxis] + T1[2].Position[SplitAxis]) / 3.0f;
        float c2 = (T2[0].Position[SplitAxis] + T2[1].Position[SplitAxis] + T2[2].Position[SplitAxis]) / 3.0f;
        return c1 < c2;
    });

    auto SplitPos_Iter = TIter_Start + (TIter_End - TIter_Start) / 2;

    int CurrentBoundingIndex = (int)BVH_Array.size();
    BVH_Array.push_back(MeshBoundingBox);

    int Child1 = BuildBVHBox_Recursive(BVH_Array, MeshStart, TIter_Start, SplitPos_Iter, TrianglePerBox);
    int Child2 = BuildBVHBox_Recursive(BVH_Array, MeshStart, SplitPos_Iter, TIter_End, TrianglePerBox);

    BVH_Array[CurrentBoundingIndex].ChildIndex1 = Child1;
    BVH_Array[CurrentBoundingIndex].ChildIndex2 = Child2;

    return CurrentBoundingIndex;
}

template<typename VertexType, typename IndexType>
void BuildBVHOnMesh(TMesh<VertexType, IndexType>& InOutMesh, std::vector<BVHBox<typename VertexType, IndexType>>& OutBVHs) {
    std::vector<std::array<VertexType, 3>> Triangles;
    Triangles.resize(InOutMesh.Vertices.size() / 3);
    for (int i = 0; i < (int)Triangles.size(); i++) {
        Triangles[i][0] = InOutMesh.Vertices[3 * i + 0];
        Triangles[i][1] = InOutMesh.Vertices[3 * i + 1];
        Triangles[i][2] = InOutMesh.Vertices[3 * i + 2];
    }
    OutBVHs.clear();
    BuildBVHBox_Recursive<VertexType, IndexType>(OutBVHs, Triangles.begin(), Triangles.begin(), Triangles.end(), 10);
    for (int i = 0; i < (int)Triangles.size(); i++) {
        InOutMesh.Vertices[3 * i + 0] = Triangles[i][0];
        InOutMesh.Vertices[3 * i + 1] = Triangles[i][1];
        InOutMesh.Vertices[3 * i + 2] = Triangles[i][2];
    }
}

template <typename VertexType, typename IndexType>
TMesh<VertexType, IndexType> TMesh<VertexType, IndexType>::LoadObj(const std::string ObjFileName)
{
	TMesh<VertexType, IndexType> OutMesh;
	tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!CoreGeometryImpl::TinyObjLoadObj(&attrib, &shapes, &materials, &err, ObjFileName.c_str(), nullptr, false)) {
        throw std::runtime_error(warn + err);
    }

    for (const auto& shape : shapes) {
        for (const auto& num_verts : shape.mesh.num_face_vertices) {
            if(num_verts!=3)
            {
	            throw std::runtime_error("Can only support triangulate mesh!");
            }
        }
    }

    uint32_t CurrentIndex = 0;
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            VertexType vertex{};
	        vertex.Position = {
	        	attrib.vertices[3 * index.vertex_index],
	        	attrib.vertices[3 * index.vertex_index + 1],
	        	attrib.vertices[3 * index.vertex_index + 2]};
	        if(attrib.normals.size()>0)
	        {
				vertex.Normal = {
					attrib.normals[3 * index.normal_index],
					attrib.normals[3 * index.normal_index + 1],
					attrib.normals[3 * index.normal_index + 2]};
	        }
    		else
	        {
		        vertex.Normal = {};
	        }

	        if(attrib.texcoords.size()>0)
	        {
				vertex.TexCoord = {
					attrib.texcoords[2 * index.texcoord_index],
                    1. - attrib.texcoords[2 * index.texcoord_index + 1] };
	        }
    		else
	        {
		        vertex.TexCoord = {};
	        }

	        vertex.Color = {};
	        vertex.TangentTypeX = {};
	        vertex.TangentTypeY = {};
            OutMesh.Vertices.push_back(vertex);
            OutMesh.Indices.push_back(CurrentIndex++);
        }
    }

    return OutMesh;
}

template <typename VertexType, typename IndexType>
void CalculateNormal(TMesh<VertexType, IndexType>& InOutMesh, bool bSmooth = true)
{
    for (auto& v : InOutMesh.Vertices)
    {
        v.Normal = {};
    }

    if (bSmooth)
    {
        auto pos_cmp = [](const typename VertexType::PositionType& a, const typename VertexType::PositionType& b) {
            for (int i = 0; i < 3; i++) {
                if (a[i] != b[i]) return a[i] < b[i];
            }
            return false;
        };
        std::map<typename VertexType::PositionType, typename VertexType::NormalType, decltype(pos_cmp)> posNormalMap(pos_cmp);

        for (size_t i = 0; i < InOutMesh.Vertices.size() / 3; i++)
        {
            auto v0 = InOutMesh.Vertices[3 * i].Position;
            auto v1 = InOutMesh.Vertices[3 * i + 1].Position;
            auto v2 = InOutMesh.Vertices[3 * i + 2].Position;
            auto faceNormal = cross(v1 - v0, v2 - v0);
            posNormalMap[v0] = posNormalMap.count(v0) ? posNormalMap[v0] + faceNormal : faceNormal;
            posNormalMap[v1] = posNormalMap.count(v1) ? posNormalMap[v1] + faceNormal : faceNormal;
            posNormalMap[v2] = posNormalMap.count(v2) ? posNormalMap[v2] + faceNormal : faceNormal;
        }

        for (size_t i = 0; i < InOutMesh.Vertices.size(); i++)
        {
            auto& normal = posNormalMap[InOutMesh.Vertices[i].Position];
            if (length(normal) > 1e-6)
                InOutMesh.Vertices[i].Normal = normalize(normal);
        }
    }
    else
    {
        for (size_t i = 0; i < InOutMesh.Vertices.size() / 3; i++)
        {
            auto v0 = InOutMesh.Vertices[3 * i].Position;
            auto v1 = InOutMesh.Vertices[3 * i + 1].Position;
            auto v2 = InOutMesh.Vertices[3 * i + 2].Position;
            auto faceNormal = cross(v1 - v0, v2 - v0);
            if (length(faceNormal) > 1e-6)
                faceNormal = normalize(faceNormal);

            InOutMesh.Vertices[3 * i].Normal = faceNormal;
            InOutMesh.Vertices[3 * i + 1].Normal = faceNormal;
            InOutMesh.Vertices[3 * i + 2].Normal = faceNormal;
        }
    }
}


