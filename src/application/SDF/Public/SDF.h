#pragma once

#include <CoreMath.inl>

namespace SDF 
{
	class Transform
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
    float SphereSD(const float3& measurePoint, float radius, const float4x4& invTransformationMat);


	// Box
	// extent is the half size of the box's boundary
	float BoxSD(const float3& measurePoint, const float3& extent, const float4x4& invTransformationMat);


	// Primitive Combination
	float Union(float d1, float d2);
	float Intersection(float d1, float d2);
	float Subtraction(float base, float subtrahend);
	float Xor(float d1, float d2);

}