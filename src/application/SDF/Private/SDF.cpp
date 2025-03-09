#include "SDF.h"
// #include <glm/gtx/extended_min_max.inl>

constexpr float3 vec3_zero =	float3(0);
constexpr float3 x_axis =		float3(1, 0, 0);
constexpr float3 y_axis =		float3(0, 1, 0);
constexpr float3 z_axis =		float3(0, 0, 1);
constexpr float4x4 iMat4x4 =	float4x4(1);

#define TRANSLATE(mat, vec)		( glm::translate(mat, vec) )
#define ROTATE(mat, vec)		( glm::rotate(glm::rotate(glm::rotate(mat, (vec).x, x_axis), (vec).y, y_axis), (vec).z, z_axis) )
#define SCALE(mat, vec)			( glm::scale(mat, vec) )

#define LENGTH(x)		( glm::length(x) )
#define NORMALIZE(x)	( glm::normalize(x) )
#define MAX(x, y)		( glm::max(x, y) )
#define MIN(x, y)		( glm::min(x, y) )




// Transform

void SDF::Transform::updateTransformationMatrix()
{
	m_transformationMatrix = TRANSLATE(ROTATE(SCALE(iMat4x4, m_scale), m_rotation), m_position);
}

void SDF::Transform::SetPosition(const float3& position)
{
	m_position = position;
	updateTransformationMatrix();
}

void SDF::Transform::SetRotation(const float3& rotation)
{
	m_rotation = rotation;
	updateTransformationMatrix();
}

void SDF::Transform::SetScale(const float3& scale)
{
	m_scale = scale;
	updateTransformationMatrix();
}




// Sphere
float SDF::SphereSD(const float3& measurePoint, float radius, const float4x4& invTransformationMat)
{
	float3 pInObjSpace = invTransformationMat * float4(measurePoint, 1);
	return LENGTH(pInObjSpace) - radius;
}

// Box
float SDF::BoxSD(const float3& measurePoint, const float3& extent, const float4x4& invTransformationMat)
{
	float3 pInObjSpace = invTransformationMat * float4(measurePoint, 1);

	float3 displacement = abs(pInObjSpace) - extent;

	// hack to avoid branching
	float outsideCaseDist = LENGTH(MAX(displacement, vec3_zero));
	float insideCaseDist = MIN(MAX(displacement.x, displacement.y), 0.0f);

	return outsideCaseDist + insideCaseDist;
}


// Primitive Combination
float SDF::Union(float d1, float d2)
{
	return MIN(d1, d2);
}

float SDF::Intersection(float d1, float d2)
{
	return MAX(d1, d2);
}

float SDF::Subtraction(float base, float subtrahend)
{
	return MAX(base, -subtrahend);
}

float SDF::Xor(float d1, float d2)
{
	return MAX(MIN(d1, d2), -MAX(d1, d2));
}

int main()
{
	// Sphere


	// Box
}