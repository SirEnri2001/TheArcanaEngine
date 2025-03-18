#define SDF_IMPLEMENT
#include "SDF.h"

constexpr float3 vec3_zero = float3(0);
constexpr float3 x_axis = float3(1, 0, 0);
constexpr float3 y_axis = float3(0, 1, 0);
constexpr float3 z_axis = float3(0, 0, 1);
constexpr float4x4 iMat4x4 = float4x4(1);


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
	float3 displacement = abs(measurePoint) - extent;
	return glm::length(glm::max(displacement, float3(0))) + glm::min(glm::max(displacement.x, displacement.y), 0.0f);
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

// Helper function to calculate normal
float3 CalculateNormal(ISignedDistance* sdf, const float3& point, float epsilon = 0.001f)
{
	float3 normal;
	normal.x = sdf->Sample(point + float3(epsilon, 0, 0)) - sdf->Sample(point - float3(epsilon, 0, 0));
	normal.y = sdf->Sample(point + float3(0, epsilon, 0)) - sdf->Sample(point - float3(0, epsilon, 0));
	normal.z = sdf->Sample(point + float3(0, 0, epsilon)) - sdf->Sample(point - float3(0, 0, epsilon));
	return NORMALIZE(normal);
}

// BoxSDF Implementation
float BoxSDF::Sample(float3 Point)
{
	float3 displacement = abs(Point) - float3(1.);
	return glm::length(glm::max(displacement, float3(0))) + glm::min(glm::max(displacement.x, displacement.y), 0.0f);
}

float BoxSDF::Sample(float3 Point, float4x4 InvModelMatrix)
{
	float4 NewPoint = float4(Point, 1.) * InvModelMatrix;
	Point = NewPoint;
	return Sample(Point);
}

bool BoxSDF::IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float maxDistance = 100.0f;
	float t = 0.0f;

	for (int i = 0; i < 128; i++)
	{
		float3 p = Point + RayDirection * t;
		float d = Sample(p);

		if (d < 0.001f) return t;
		if (t > maxDistance) return false;

		t += IsBidirectional ? d : abs(d);
	}
	return false;
}

bool BoxSDF::IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float t = IntersectRay(OutDistance, Point, RayDirection, IsBidirectional);
	if (t >= 0)
	{
		float3 hitPoint = Point + RayDirection * t;
		OutNormal = CalculateNormal(this, hitPoint);
	}
	return t;
}

// SphereSDF Implementation
float SphereSDF::Sample(float3 Point)
{
	return glm::length(Point);
}

float SphereSDF::Sample(float3 Point, float4x4 InvModelMatrix)
{
	return SDF::SphereSD(Point, InvModelMatrix, 0.5f);
}

bool SphereSDF::IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float maxDistance = 100.0f;
	float t = 0.0f;

	for (int i = 0; i < 128; i++)
	{
		float3 p = Point + RayDirection * t;
		float d = Sample(p);

		if (d < 0.001f) return t;
		if (t > maxDistance) return false;

		t += IsBidirectional ? d : abs(d);
	}
	return false;
}

bool SphereSDF::IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float t = IntersectRay(OutDistance, Point, RayDirection, IsBidirectional);
	if (t >= 0)
	{
		float3 hitPoint = Point + RayDirection * t;
		OutNormal = CalculateNormal(this, hitPoint);
	}
	return t;
}

// Capsule Implementation
float CapsuleSDF::Sample(float3 Point)
{
	return SDF::CapsuleSD(Point, float4x4(1), 0.5f, 0.25f);
}

float CapsuleSDF::Sample(float3 Point, float4x4 InvModelMatrix)
{
	return SDF::CapsuleSD(Point, InvModelMatrix, 0.5f, 0.25f);
}

bool CapsuleSDF::IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float maxDistance = 100.0f;
	float t = 0.0f;

	for (int i = 0; i < 128; i++)
	{
		float3 p = Point + RayDirection * t;
		float d = Sample(p);

		if (d < 0.001f) return t;
		if (t > maxDistance) return false;

		t += IsBidirectional ? d : abs(d);
	}
	return false;
}

bool CapsuleSDF::IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float t = IntersectRay(OutDistance, Point, RayDirection, IsBidirectional);
	if (t >= 0)
	{
		float3 hitPoint = Point + RayDirection * t;
		OutNormal = CalculateNormal(this, hitPoint);
	}
	return t;
}

// UnionSDF Implementation
float UnionSDF::Sample(float3 Point)
{
	if (Objects.empty()) return INFINITY;

	float result = Objects[0]->Sample(Point);
	for (size_t i = 1; i < Objects.size(); i++)
	{
		result = SDF::Union(result, Objects[i]->Sample(Point));
	}
	return result;
}

float UnionSDF::Sample(float3 Point, float4x4 InvModelMatrix)
{
	if (Objects.empty()) return INFINITY;

	float result = Objects[0]->Sample(Point, InvModelMatrix);
	for (size_t i = 1; i < Objects.size(); i++)
	{
		result = SDF::Union(result, Objects[i]->Sample(Point, InvModelMatrix));
	}
	return result;
}

bool UnionSDF::IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float maxDistance = 100.0f;
	float t = 0.0f;

	for (int i = 0; i < 128; i++)
	{
		float3 p = Point + RayDirection * t;
		float d = Sample(p);

		if (d < 0.001f) return t;
		if (t > maxDistance) return false;

		t += IsBidirectional ? d : abs(d);
	}
	return false;
}

bool UnionSDF::IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float minT = INFINITY;
	float3 tempNormal;

	for (auto* obj : Objects)
	{
		float t = obj->IntersectRayWithNormal(OutDistance, tempNormal, Point, RayDirection, IsBidirectional);
		if (t >= 0 && t < minT)
		{
			minT = t;
			OutNormal = tempNormal;
		}
	}

	return minT == INFINITY ? false : minT;
}

// IntersectSDF Implementation
float IntersectSDF::Sample(float3 Point)
{
	if (Objects.empty()) return INFINITY;

	float result = Objects[0]->Sample(Point);
	for (size_t i = 1; i < Objects.size(); i++)
	{
		result = SDF::Intersection(result, Objects[i]->Sample(Point));
	}
	return result;
}

float IntersectSDF::Sample(float3 Point, float4x4 InvModelMatrix)
{
	if (Objects.empty()) return INFINITY;

	float result = Objects[0]->Sample(Point, InvModelMatrix);
	for (size_t i = 1; i < Objects.size(); i++)
	{
		result = SDF::Intersection(result, Objects[i]->Sample(Point, InvModelMatrix));
	}
	return result;
}

bool IntersectSDF::IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float maxDistance = 100.0f;
	float t = 0.0f;

	for (int i = 0; i < 128; i++)
	{
		float3 p = Point + RayDirection * t;
		float d = Sample(p);

		if (d < 0.001f) return t;
		if (t > maxDistance) return false;

		t += IsBidirectional ? d : abs(d);
	}
	return false;
}

bool IntersectSDF::IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float t = IntersectRay(OutDistance, Point, RayDirection, IsBidirectional);
	if (t >= 0)
	{
		float3 hitPoint = Point + RayDirection * t;
		OutNormal = CalculateNormal(this, hitPoint);
	}
	return t;
}

// SubtractSDF Implementation
float SubtractSDF::Sample(float3 Point)
{
	if (Objects.empty()) return INFINITY;

	float result = Objects[0]->Sample(Point);
	for (size_t i = 1; i < Objects.size(); i++)
	{
		result = SDF::Subtraction(result, Objects[i]->Sample(Point));
	}
	return result;
}

float SubtractSDF::Sample(float3 Point, float4x4 InvModelMatrix)
{
	if (Objects.empty()) return INFINITY;

	float result = Objects[0]->Sample(Point, InvModelMatrix);
	for (size_t i = 1; i < Objects.size(); i++)
	{
		result = SDF::Subtraction(result, Objects[i]->Sample(Point, InvModelMatrix));
	}
	return result;
}

bool SubtractSDF::IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float maxDistance = 100.0f;
	float t = 0.0f;

	for (int i = 0; i < 128; i++)
	{
		float3 p = Point + RayDirection * t;
		float d = Sample(p);

		if (d < 0.001f) return t;
		if (t > maxDistance) return false;

		t += IsBidirectional ? d : abs(d);
	}
	return false;
}

bool SubtractSDF::IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float t = IntersectRay(OutDistance, Point, RayDirection, IsBidirectional);
	if (t >= 0)
	{
		float3 hitPoint = Point + RayDirection * t;
		OutNormal = CalculateNormal(this, hitPoint);
	}
	return t;
}

// XorSDF Implementation
float XorSDF::Sample(float3 Point)
{
	if (Objects.empty()) return INFINITY;

	float result = Objects[0]->Sample(Point);
	for (size_t i = 1; i < Objects.size(); i++)
	{
		result = SDF::Xor(result, Objects[i]->Sample(Point));
	}
	return result;
}

float XorSDF::Sample(float3 Point, float4x4 InvModelMatrix)
{
	if (Objects.empty()) return INFINITY;

	float result = Objects[0]->Sample(Point, InvModelMatrix);
	for (size_t i = 1; i < Objects.size(); i++)
	{
		result = SDF::Xor(result, Objects[i]->Sample(Point, InvModelMatrix));
	}
	return result;
}

bool XorSDF::IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float maxDistance = 100.0f;
	float t = 0.0f;

	for (int i = 0; i < 128; i++)
	{
		float3 p = Point + RayDirection * t;
		float d = Sample(p);

		if (d < 0.001f) return t;
		if (t > maxDistance) return false;

		t += IsBidirectional ? d : abs(d);
	}
	return false;
}

bool XorSDF::IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float t = IntersectRay(OutDistance, Point, RayDirection, IsBidirectional);
	if (t >= 0)
	{
		float3 hitPoint = Point + RayDirection * t;
		OutNormal = CalculateNormal(this, hitPoint);
	}
	return t;
}

// BoxSDF constructors and destructor
BoxSDF::BoxSDF()
{
}

BoxSDF::~BoxSDF()
{
}

// SphereSDF constructors and destructor
SphereSDF::SphereSDF()
{
}

SphereSDF::~SphereSDF()
{
}

// Capsule constructors and destructor
CapsuleSDF::CapsuleSDF()
{
}

CapsuleSDF::~CapsuleSDF()
{
}

// UnionSDF constructors and destructor
UnionSDF::UnionSDF()
{
}

UnionSDF::~UnionSDF()
{
	for (auto* obj : Objects)
	{
		delete obj;
	}
	Objects.clear();
}

// IntersectSDF constructors and destructor
IntersectSDF::IntersectSDF()
{
}

IntersectSDF::~IntersectSDF()
{
	for (auto* obj : Objects)
	{
		delete obj;
	}
	Objects.clear();
}

// SubtractSDF constructors and destructor
SubtractSDF::SubtractSDF()
{
}

SubtractSDF::~SubtractSDF()
{
	for (auto* obj : Objects)
	{
		delete obj;
	}
	Objects.clear();
}

// XorSDF constructors and destructor
XorSDF::XorSDF()
{
}

XorSDF::~XorSDF()
{
	for (auto* obj : Objects)
	{
		delete obj;
	}
	Objects.clear();
}

// BoxSDF copy constructor
BoxSDF::BoxSDF(const BoxSDF& other) : ISignedDistance(other)
{
}

// SphereSDF copy constructor
SphereSDF::SphereSDF(const SphereSDF& other) : ISignedDistance(other)
{
}

// Capsule copy constructor
CapsuleSDF::CapsuleSDF(const CapsuleSDF& other) : ISignedDistance(other)
{
}

// UnionSDF copy constructor
UnionSDF::UnionSDF(const UnionSDF& other) : ISignedDistance(other)
{
}

// IntersectSDF copy constructor
IntersectSDF::IntersectSDF(const IntersectSDF& other) : ISignedDistance(other)
{
}

// SubtractSDF copy constructor
SubtractSDF::SubtractSDF(const SubtractSDF& other) : ISignedDistance(other)
{
}

// XorSDF copy constructor
XorSDF::XorSDF(const XorSDF& other) : ISignedDistance(other)
{
}

// PlaneSDF Implementation
namespace
{
	constexpr float PLANE_EPSILON = 0.001f;
}

// Constructors and destructor
PlaneSDF::PlaneSDF() : m_normal(float3(0, 1, 0)), m_distance(0.0f)
{
}

PlaneSDF::PlaneSDF(const PlaneSDF& other) :
	ISignedDistance(other),
	m_normal(other.m_normal),
	m_distance(other.m_distance)
{
}

PlaneSDF::~PlaneSDF()
{
}

float PlaneSDF::Sample(float3 Point)
{
	// 平面SDF的计算公式：点到平面的有向距离
	return glm::dot(Point, m_normal) - m_distance;
}

float PlaneSDF::Sample(float3 Point, float4x4 InvModelMatrix)
{
	float4 transformedPoint = InvModelMatrix * float4(Point, 1.0f);
	return Sample(float3(transformedPoint));
}

bool PlaneSDF::IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	float denominator = glm::dot(m_normal, RayDirection);

	if (abs(denominator) < PLANE_EPSILON)
	{
		return false;
	}

	float t = (m_distance - glm::dot(m_normal, Point)) / denominator;

	if (IsBidirectional || t >= 0.0f)
	{
		OutDistance = t;
		return true;
	}

	return false;
}

bool PlaneSDF::IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional)
{
	if (IntersectRay(OutDistance, Point, RayDirection, IsBidirectional))
	{
		OutNormal = m_normal;
		if (glm::dot(RayDirection, m_normal) > 0)
		{
			OutNormal = -OutNormal;
		}
		return true;
	}
	return false;
}