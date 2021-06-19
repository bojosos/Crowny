#pragma once

#include "Crowny/Renderer/Texture.h"
#include "Crowny/Renderer/TextureManager.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/MeshFactory.h"

#include "Crowny/Scene/SceneCamera.h"
#include "../../Crowny-Editor/Source/Panels/ImGuiMaterialPanel.h" // ?

#include "Crowny/Scripting/CWMonoRuntime.h"

#include "Crowny/Common/Color.h"
#include "Crowny/Ecs/Entity.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <entt/entt.hpp>

namespace Crowny
{
	template <class Component>
	void ComponentEditorWidget(Entity entity);

	struct Component
	{
		Entity ComponentParent;
    MonoObject* ManagedInstance = nullptr;
	};

	struct TagComponent : public Component
	{
		std::string Tag = "";

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag) : Tag(tag) {}

		operator std::string& () { return Tag; }
		operator const std::string& () const { return Tag; }
	};

	struct TransformComponent : public Component
	{
		glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& position) : Position(position) {}

		glm::mat4 GetTransform() const
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));

			return glm::translate(glm::mat4(1.0f), Position)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}

	};

	template <>
	void ComponentEditorWidget<TransformComponent>(Entity e);

	struct CameraComponent : public Component
	{
		SceneCamera Camera;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	template <>
	void ComponentEditorWidget<CameraComponent>(Entity e);

	struct TextComponent : public Component
	{
		std::string Text = "";
		Ref<::Crowny::Font> Font;
		glm::vec4 Color{ 0.0f, 0.3f, 0.3f, 1.0f };
		//Crowny::Material Material;

		TextComponent() { Font = FontManager::Get("default"); };
		TextComponent(const TextComponent&) = default;
		TextComponent(const std::string& text) : Text(text) {}
	};

	template <>
	void ComponentEditorWidget<TextComponent>(Entity e);

	struct SpriteRendererComponent : public Component
	{
		Ref<Texture2D> Texture;
		glm::vec4 Color;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const Ref<Texture2D>& texture, Crowny::Color color) : Texture(texture), Color(color) {}
	};

	template <>
	void ComponentEditorWidget<SpriteRendererComponent>(Entity e);

	struct MeshRendererComponent : public Component
	{
		Ref<::Crowny::Mesh> Mesh;

		MeshRendererComponent() { Mesh = MeshFactory::CreateSphere(); Mesh->SetMaterialInstnace(CreateRef<MaterialInstance>(ImGuiMaterialPanel::GetSlectedMaterial())); };
		MeshRendererComponent(const MeshRendererComponent&) = default;
	};

	template <>
	void ComponentEditorWidget<MeshRendererComponent>(Entity e);

	struct RelationshipComponent : public Component
	{
		std::vector<Entity> Children;
		Entity Parent;

		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent&) = default;
		RelationshipComponent(const Entity& parent) : Parent(parent) { }
	};

	struct MonoScriptComponent : public Component
	{
		CWMonoClass* Class = nullptr;
		MonoObject* Instance = nullptr;
		CWMonoMethod* OnUpdate = nullptr;
		CWMonoMethod* OnStart = nullptr;
		CWMonoMethod* OnDestroy = nullptr;
    	std::vector<CWMonoField*> DisplayableFields;
    	std::vector<CWMonoProperty*> DisplayableProperties;

		MonoScriptComponent() = default;
		MonoScriptComponent(const MonoScriptComponent&) = default;

		MonoScriptComponent(const std::string& name)
		{
			Class = CWMonoRuntime::GetClientAssembly()->GetClass("Sandbox", name);
			if (Class)
			{
				CW_ENGINE_INFO("Here");
				for (auto* field : Class->GetFields())
				{
					if (field && !field->HasAttribute(CWMonoRuntime::GetBuiltinClasses().HideInInspector)
						&& ((field->GetVisibility() == CWMonoVisibility::Public)
							|| field->HasAttribute(CWMonoRuntime::GetBuiltinClasses().SerializeFieldAttribute)
							|| field->HasAttribute(CWMonoRuntime::GetBuiltinClasses().ShowInInspector)))
					{
						DisplayableFields.push_back(field);
					}
				}

				for (auto* prop : Class->GetProperties())
				{
					if (prop && prop->HasAttribute(CWMonoRuntime::GetBuiltinClasses().HideInInspector)
						&& ((prop->GetVisibility() == CWMonoVisibility::Public)
							|| prop->HasAttribute(CWMonoRuntime::GetBuiltinClasses().SerializeFieldAttribute)
							|| prop->HasAttribute(CWMonoRuntime::GetBuiltinClasses().ShowInInspector)))
					{
						DisplayableProperties.push_back(prop);
					}
				}
				CW_ENGINE_INFO("After Here");
			}
		}
	};

	template <>
	void ComponentEditorWidget<MonoScriptComponent>(Entity e);
}
