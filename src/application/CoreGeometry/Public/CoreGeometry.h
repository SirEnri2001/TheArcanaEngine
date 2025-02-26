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

#include "tiny_obj_loader.h"

namespace CoreGeometryImpl
{
	bool COREGEOMETRY_API TinyObjLoadObj(tinyobj::attrib_t *attrib, std::vector<tinyobj::shape_t> *shapes,
             std::vector<tinyobj::material_t> *materials, std::string *err,
             const char *filename, const char *mtl_basedir = nullptr,
             bool triangulate = true);
}


template<typename PositionType, typename ColorType, typename TexCoordType, typename NormalType, typename TangentType>
struct TVertex {
    PositionType Position;
    ColorType Color;
    TexCoordType TexCoord;
    NormalType Normal;
    TangentType TangentTypeX;
    TangentType TangentTypeY;
};

template<typename VertexType, typename IndexType>
class TMesh
{
public:
    std::vector<VertexType> Vertices;
    std::vector<IndexType> Indices;

    static TMesh<VertexType, IndexType> LoadObj(const std::string ObjFileName);
};


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
void CalculateNormal(TMesh<VertexType, IndexType>& InOutMesh)
{
    for (size_t i = 0; i < InOutMesh.Vertices.size() / 3; i++)
    {
        auto v1 = InOutMesh.Vertices[3 * i + 1].Position - InOutMesh.Vertices[3 * i].Position;
        auto v2 = InOutMesh.Vertices[3 * i + 2].Position - InOutMesh.Vertices[3 * i].Position;
        InOutMesh.Vertices[3 * i].Normal += cross(v1, v2);
        InOutMesh.Vertices[3 * i + 1].Normal += cross(v1, v2);
        InOutMesh.Vertices[3 * i + 2].Normal += cross(v1, v2);
    }

    for (size_t i = 0; i < InOutMesh.Vertices.size(); i++)
    {
        InOutMesh.Vertices[i].Normal = normalize(InOutMesh.Vertices[i].Normal);
    }
}


