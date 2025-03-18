#pragma once

#if defined(SDF_IMPLEMENT)
	#define SDF_API __declspec(dllexport)
#elif defined(SDF_INCLUDE)
	#define SDF_API __declspec(dllimport)
#else
	#error Please specify API linkage before include this file
	#define SDF_API
#endif

#include <CoreMath.inl>

namespace SDF 
{
	class SDF_API Transform
	{
	private:
		float3 m_position;
		float3 m_rotation;	// Rotation is in the form of euler angles in "DEGREE" in the sequence of "XYZ".
		float3 m_scale;

		float4x4 m_transformationMatrix;

		void updateTransformationMatrix();

	public:
		Transform(float3 position, float3 rotation, float3 scale) : m_position(position), m_rotation(rotation), m_scale(scale), m_transformationMatrix(0)
		{
			updateTransformationMatrix();
		}

		auto position()	const -> const float3& { return m_position; }
		auto rotation()	const -> const float3& { return m_rotation; }
		auto scale()	const -> const float3& { return m_scale; }
		auto GetTransformationMatrix() const -> const float4x4& { return m_transformationMatrix; }

		void SetPosition(const float3& position);
		void SetRotation(const float3& rotation);
		void SetScale(const float3& scale);
	};


	/*****************************************************************************************************************************************/
	/*					SDFs																												 */
	/*****************************************************************************************************************************************/

	// Sphere
	SDF_API float SphereSD(const float3& measurePoint, const float4x4& shapeInvMat, float radius);

	// Box
	// extent is the half size of the box's boundary
	SDF_API float BoxSD(const float3& measurePoint, const float4x4& shapeInvMat, const float3& extent);

	// Vertical CapsuleSDF
	SDF_API float CapsuleSD(const float3& measurePoint, const float4x4& shapeInvMat, float halfHeight, float radius);

	// Primitive Combination
	SDF_API float Union(float d1, float d2);
	SDF_API float Intersection(float d1, float d2);
	SDF_API float Subtraction(float base, float subtrahend);
	SDF_API float Xor(float d1, float d2);

}

class SDF_API ISignedDistance
{
public:
	ISignedDistance() = default;
	virtual ~ISignedDistance() = default;
	ISignedDistance(const ISignedDistance&) = default;

	virtual float Sample(float3 Point) = 0;
	virtual float Sample(float3 Point, float4x4 InvModelMatrix) = 0;
	virtual bool IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional) = 0;
	virtual bool IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional) = 0;
};

class SDF_API BoxSDF : public ISignedDistance
{
public:
	BoxSDF();
	BoxSDF(const BoxSDF& other);
	virtual ~BoxSDF() override;
	float Sample(float3 Point) override;
	float Sample(float3 Point, float4x4 InvModelMatrix) override;
	bool IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional) override;
	bool IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional) override;
};

class SDF_API SphereSDF : public ISignedDistance
{
public:
	SphereSDF();
	SphereSDF(const SphereSDF& other);
	virtual ~SphereSDF() override;
	float Sample(float3 Point) override;
	float Sample(float3 Point, float4x4 InvModelMatrix) override;
	bool IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional) override;
	bool IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional) override;
};

class SDF_API CapsuleSDF : public ISignedDistance
{
public:
	CapsuleSDF();
	CapsuleSDF(const CapsuleSDF& other);
	virtual ~CapsuleSDF() override;
	float Sample(float3 Point) override;
	float Sample(float3 Point, float4x4 InvModelMatrix) override;
	bool IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional) override;
	bool IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional) override;
};

class SDF_API PlaneSDF : public ISignedDistance
{
public:
	PlaneSDF();
	PlaneSDF(const PlaneSDF& other);
	virtual ~PlaneSDF() override;
	
	float Sample(float3 Point) override;
	float Sample(float3 Point, float4x4 InvModelMatrix) override;
	bool IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional) override;
	bool IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional) override;

private:
	float3 m_normal;   // 平面法线
	float m_distance;  // 平面到原点的距离
};

class SDF_API UnionSDF : public ISignedDistance
{
public:
	UnionSDF();
	UnionSDF(const UnionSDF& other);
	virtual ~UnionSDF() override;
	std::vector<ISignedDistance*> Objects;
	float Sample(float3 Point) override;
	float Sample(float3 Point, float4x4 InvModelMatrix) override;
	bool IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional) override;
	bool IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional) override;
};

class SDF_API IntersectSDF : public ISignedDistance
{
public:
	IntersectSDF();
	IntersectSDF(const IntersectSDF& other);
	virtual ~IntersectSDF() override;
	std::vector<ISignedDistance*> Objects;
	float Sample(float3 Point) override;
	float Sample(float3 Point, float4x4 InvModelMatrix) override;
	bool IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional) override;
	bool IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional) override;
};

class SDF_API SubtractSDF : public ISignedDistance
{
public:
	SubtractSDF();
	SubtractSDF(const SubtractSDF& other);
	virtual ~SubtractSDF() override;
	std::vector<ISignedDistance*> Objects;
	float Sample(float3 Point) override;
	float Sample(float3 Point, float4x4 InvModelMatrix) override;
	bool IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional) override;
	bool IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional) override;
};

class SDF_API XorSDF : public ISignedDistance
{
public:
	XorSDF();
	XorSDF(const XorSDF& other);
	virtual ~XorSDF() override;
	std::vector<ISignedDistance*> Objects;
	float Sample(float3 Point) override;
	float Sample(float3 Point, float4x4 InvModelMatrix) override;
	bool IntersectRay(float& OutDistance, float3 Point, float3 RayDirection, bool IsBidirectional) override;
	bool IntersectRayWithNormal(float& OutDistance, float3& OutNormal, float3 Point, float3 RayDirection, bool IsBidirectional) override;
};

class SDF_API SDFRenderProxy
{
public:

};