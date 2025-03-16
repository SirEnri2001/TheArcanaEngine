#define SDF_IMPLEMENT
#include "SDF.h"

constexpr float3 vec3_zero =	float3(0);
constexpr float3 x_axis =		float3(1, 0, 0);
constexpr float3 y_axis =		float3(0, 1, 0);
constexpr float3 z_axis =		float3(0, 0, 1);
constexpr float4x4 iMat4x4 =	float4x4(1);


#define TRANSLATE(mat, vec)		( glm::translate(mat, vec) )
#define ROTATE(mat, vec)		( glm::rotate(glm::rotate(glm::rotate(mat, (vec).x, x_axis), (vec).y, y_axis), (vec).z, z_axis) )
#define SCALE(mat, vec)			( glm::scale(mat, vec) )

#define LENGTH(x)				( glm::length(x) )
#define NORMALIZE(x)			( glm::normalize(x) )
#define MAX(x, y)				( glm::max(x, y) )
#define MIN(x, y)				( glm::min(x, y) )
#define CLAMP(val, min, max)	( glm::clamp(val, min, max) )


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
float SDF::SphereSD(const float3& measurePoint, const float4x4& shapeInvMat, float radius)
{
	float3 pInObjSpace = shapeInvMat * float4(measurePoint, 1);
	return LENGTH(pInObjSpace) - radius;
}


// Box  
float SDF::BoxSD(const float3& measurePoint, const float4x4& shapeInvMat, const float3& extent)
{
	float3 pInObjSpace = shapeInvMat * float4(measurePoint, 1);

	float3 displacement = abs(pInObjSpace) - extent;

	// hack to avoid branching between outside and inside SDF evaluation
	float outsideCaseDist = LENGTH(MAX(displacement, vec3_zero));
	float insideCaseDist = MIN(MAX(displacement.x, displacement.y), 0.0f);

	return outsideCaseDist + insideCaseDist;
}


// Vertical Capsule
float SDF::CapsuleSD(const float3& measurePoint, const float4x4& shapeInvMat, float halfHeight, float radius)
{
	float3 pInObjSpace = shapeInvMat * float4(measurePoint, 1);

	float3 p = pInObjSpace;
	p.y = MAX(0.f, (abs(p.y) - halfHeight));

	return LENGTH(p) - radius;
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