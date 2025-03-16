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

	// Vertical Capsule
	SDF_API float CapsuleSD(const float3& measurePoint, const float4x4& shapeInvMat, float halfHeight, float radius);

	// Primitive Combination
	SDF_API float Union(float d1, float d2);
	SDF_API float Intersection(float d1, float d2);
	SDF_API float Subtraction(float base, float subtrahend);
	SDF_API float Xor(float d1, float d2);

}