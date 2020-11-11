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

	template <class Component>
	void ComponentEditorWidget(Entity& e);

	struct TagComponent
	{
		std::string Tag = "";

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag) : Tag(tag) {}

		operator std::string& () { return Tag; }
		operator const std::string& () const { return Tag; }
	};

	struct TransformComponent
	{
		glm::mat4 Transform{ 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::mat4& transform) : Transform(transform) {}

		operator glm::mat4& () { return Transform; }
		operator const glm::mat4& () const { return Transform; }
	};

	template <>
	void ComponentEditorWidget<TransformComponent>(Entity& e);

	struct CameraComponent
	{
		::Crowny::Camera Camera;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	template <>
	void ComponentEditorWidget<CameraComponent>(Entity& e);

	struct TextComponent
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

	struct SpriteRendererComponent
	{
		Ref<Texture2D> Texture;
		glm::vec4 Color;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const Ref<Texture2D>& texture, Crowny::Color color) : Texture(texture), Color(color) {}
	};

	template <>
	void ComponentEditorWidget<SpriteRendererComponent>(Entity& e);

	struct MeshRendererComponent
	{
		Ref<::Crowny::Mesh> Mesh;

		MeshRendererComponent() { Mesh = MeshFactory::CreateSphere(); Mesh->SetMaterialInstnace(CreateRef<MaterialInstance>(ImGuiMaterialPanel::GetSlectedMaterial())); };
		MeshRendererComponent(const MeshRendererComponent&) = default;
	};

	template <>
	void ComponentEditorWidget<MeshRendererComponent>(Entity& e);

	struct RelationshipComponent
	{
		std::vector<Entity> Children;
		Entity Parent;

		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent&) = default;
		RelationshipComponent(const Entity& parent) : Parent(parent) { }
	};

	struct MonoScriptComponent
	{
		std::string Name;
		CWMonoClass* Class = nullptr;
		CWMonoObject* Object = nullptr;
		
		MonoScriptComponent() = default;
		MonoScriptComponent(const MonoScriptComponent&) = default;

		MonoScriptComponent(const std::string& name)
		{
			Class = CWMonoRuntime::GetAssembly("")->GetClass(name);
		}

	};

	template <>
	void ComponentEditorWidget<MonoScriptComponent>(Entity& e);
}
