#pragma once

#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/Texture.h"
#include "Crowny/Renderer/TextureManager.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/MeshFactory.h"
#include "../../Crowny-Editor/Source/Panels/ImGuiMaterialPanel.h"

#include "Crowny/Scripting/CWMonoRuntime.h"

#include "Crowny/Common/Color.h"
#include "Crowny/Ecs/Entity.h"

#include <entt/entt.hpp>

namespace Crowny
{

	struct Component
	{
		Entity ComponentParent;
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

		MonoObject* ManagedInstance;

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::vec3& position) : Position(position) {}

		glm::mat4 GetTransform() const
		{
				glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), Rotation.x, { 1, 0, 0})
				 					* glm::rotate(glm::mat4(1.0f), Rotation.y, { 0, 1, 0}) * 
									glm::rotate(glm::mat4(1.0f), Rotation.z, { 0, 0, 1});

				return glm::translate(glm::mat4(1.0f), Position) * rotation * glm::scale(glm::mat4(1.0f), Scale);
		}

	};

	template <>
	void ComponentEditorWidget<TransformComponent>(Entity& e);

	struct CameraComponent : public Component
	{
		::Crowny::Camera Camera;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	template <>
	void ComponentEditorWidget<CameraComponent>(Entity& e);

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
	void ComponentEditorWidget<TextComponent>(Entity& e);

	struct SpriteRendererComponent : public Component
	{
		Ref<Texture2D> Texture;
		glm::vec4 Color;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const Ref<Texture2D>& texture, Crowny::Color color) : Texture(texture), Color(color) {}
	};

	template <>
	void ComponentEditorWidget<SpriteRendererComponent>(Entity& e);

	struct MeshRendererComponent : public Component
	{
		Ref<::Crowny::Mesh> Mesh;

		MeshRendererComponent() { Mesh = MeshFactory::CreateSphere(); Mesh->SetMaterialInstnace(CreateRef<MaterialInstance>(ImGuiMaterialPanel::GetSlectedMaterial())); };
		MeshRendererComponent(const MeshRendererComponent&) = default;
	};

	template <>
	void ComponentEditorWidget<MeshRendererComponent>(Entity& e);

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
		std::string Name;
		CWMonoClass* Class = nullptr;
		MonoObject* Instance = nullptr;
		CWMonoMethod* UpdateMethod = nullptr;

		MonoScriptComponent() = default;
		MonoScriptComponent(const MonoScriptComponent&) = default;

		MonoScriptComponent(const std::string& name)
		{
			Class = CWMonoRuntime::GetClientAssembly()->GetClass(name);
		}

	};

	template <>
	void ComponentEditorWidget<MonoScriptComponent>(Entity& e);
}
