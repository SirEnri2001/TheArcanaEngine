#define CORESCENE_IMPLEMENT
#include "CoreScene.h"

#include <OCIdl.h>

#include "nlohmann/json.hpp"

#include <optional>

using namespace nlohmann;
float4x4 Object::GetLocalToWorld()
{
	if(Parent)
	{
		return Parent->GetLocalToWorld() * ToMatrix(Local);
	}
	return ToMatrix(Local);
}

float4x4 ToMatrix(const Transform& Trans)
{
	float4x4 OutputMat(1.0f);

	OutputMat = glm::scale(float4x4(1.0f), Trans.Scale) * OutputMat;
	OutputMat = GetRotationMat(Trans) * OutputMat;
	OutputMat = glm::translate(float4x4(1.0f), Trans.Translation) * OutputMat;
	return OutputMat;
}

//Transform FromMatrix(float4x4 InMat)
//{
//	// this is so hard
//	return Transform();
//}

//Transform Compose(const Transform& Parent, const Transform& Local)
//{
//	Transform OutTrans;
//	OutTrans.Rotation = Parent.Rotation * Local.Rotation;
//	OutTrans.Translation = Parent.Translation + Parent.Rotation * Local.Translation;
//	OutTrans.Scale = Parent.Scale * Local.Rotation * Local.Scale;
//
//	return OutTrans;
//}

float4x4 GetRotationMat(const Transform& Trans)
{
	float4x4 RotMat = float4x4(1.0f);
	RotMat = glm::rotate(float4x4(1.0f), Trans.RotationXYZ[0], float3(1.f, 0.f, 0.f)) * RotMat;
	RotMat = glm::rotate(float4x4(1.0f), Trans.RotationXYZ[1], float3(0.f, 1.f, 0.f)) * RotMat;
	RotMat = glm::rotate(float4x4(1.0f), Trans.RotationXYZ[2], float3(0.f,0.f,1.f)) * RotMat;
	return RotMat;
}

bool LoadTransform(json TransformJsonObject, Transform& Trans)
{
	if (TransformJsonObject["Location"].is_object() 
		&& TransformJsonObject["Location"]["x"].is_number()
		&& TransformJsonObject["Location"]["y"].is_number()
		&& TransformJsonObject["Location"]["z"].is_number())
	{
		Trans.Translation.x = TransformJsonObject["Location"]["x"].get<float>();
		Trans.Translation.y = TransformJsonObject["Location"]["y"].get<float>();
		Trans.Translation.z = TransformJsonObject["Location"]["z"].get<float>();
	}
	if (TransformJsonObject["Rotation"].is_object()
		&& TransformJsonObject["Rotation"]["x"].is_number()
		&& TransformJsonObject["Rotation"]["y"].is_number()
		&& TransformJsonObject["Rotation"]["z"].is_number())
	{
		Trans.RotationXYZ.x = TransformJsonObject["Rotation"]["x"].get<float>();
		Trans.RotationXYZ.y = TransformJsonObject["Rotation"]["y"].get<float>();
		Trans.RotationXYZ.z = TransformJsonObject["Rotation"]["z"].get<float>();
	}
	if (TransformJsonObject["Scale"].is_object()
		&& TransformJsonObject["Scale"]["x"].is_number()
		&& TransformJsonObject["Scale"]["y"].is_number()
		&& TransformJsonObject["Scale"]["z"].is_number())
	{
		Trans.Scale.x = TransformJsonObject["Scale"]["x"].get<float>();
		Trans.Scale.y = TransformJsonObject["Scale"]["y"].get<float>();
		Trans.Scale.z = TransformJsonObject["Scale"]["z"].get<float>();
	}
	return true;
}

bool LoadPrimitive(Object* InOutObject, json JsonObject, Scene& Scene);


Object* CreateObject(json JsonObject, Scene& Scene)
{
	std::unique_ptr<Object> NewObjectUptr = std::make_unique<Object>();
	Object* NewObject = NewObjectUptr.get();
	Scene.OwnObjects[Scene.OwnObjectSize] = std::move(NewObjectUptr);

	// Load Primitive info
	if(JsonObject["Type"]=="Primitive")
	{
		LoadPrimitive(NewObject, JsonObject, Scene);
	}

	// Load Children info
	if (JsonObject["Children"].is_array())
	{
		auto Children = JsonObject["Children"].get<json::array_t>();
		for (auto& Child:Children)
		{
			if (Child.is_object())
			{
				NewObject->Children.push_back(CreateObject(Child.get<json>(), Scene));
			}
		}
	}
	return NewObject;
}

bool LoadPrimitive(Object* InOutObject, json JsonObject, Scene& Scene)
{
	if (JsonObject["Mesh"].is_string())
	{
		// Create Static Mesh
		std::string PrimitiveName = JsonObject["Mesh"].get<std::string>();
		auto MeshUniquePtr = std::make_unique<Mesh>(Mesh::LoadObj("models/" + PrimitiveName));
		InOutObject->MeshInstance = MeshUniquePtr.get();
		Scene.OwnMeshes[Scene.OwnMeshSize++] = std::move(MeshUniquePtr);
	}
	if(JsonObject["Transform"].is_object())
	{
		LoadTransform(JsonObject["Transform"], InOutObject->Local);
	}
	
	return true;
}

bool Scene::LoadSceneJson(Scene& OutScene, std::string JsonString)
{

	auto SceneJsonObject = json::parse(JsonString);

	if (!SceneJsonObject.is_object())
	{
		return false;
	}

	if (SceneJsonObject["Children"].is_array())
	{
		auto Children = SceneJsonObject["Children"].get<json::array_t>();
		for (auto& Child:Children)
		{
			if (Child.is_object())
			{
				OutScene.Children.push_back(CreateObject(Child.get<json>(), OutScene));
			}
		}
	}
	return true;
}

Object::~Object()
{
	
}