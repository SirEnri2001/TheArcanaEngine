#define TINYOBJLOADER_IMPLEMENTATION
#define COREGEOMETRY_IMPLEMENT

#include "CoreGeometry.h"

bool CoreGeometryImpl::TinyObjLoadObj(tinyobj::attrib_t *attrib, std::vector<tinyobj::shape_t> *shapes,
             std::vector<tinyobj::material_t> *materials, std::string *err,
             const char *filename, const char *mtl_basedir,
             bool triangulate)
{
	return tinyobj::LoadObj(attrib, shapes, materials, err, filename, mtl_basedir, triangulate);
}