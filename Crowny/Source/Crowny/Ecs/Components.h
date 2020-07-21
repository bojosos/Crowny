#pragma once

#include <entt/entt.hpp>
#include "Crowny/ImGui/ImGuiComponentEditor.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
	struct TransformComponent
	{
		glm::mat4 Transform;

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(const glm::mat4 & transform)
			: Transform(transform) {}

		operator glm::mat4& () { return Transform; }
		operator const glm::mat4& () const { return Transform; }
	};

	struct CameraComponent
	{
		CameraProperties Camera;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
		CameraComponent(const CameraProperties& camera)
			: Camera(camera) {}
	};

	struct SpriteRendererComponent
	{
		Ref<Texture2D> Texture;
		Color Color;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const Ref<Texture2D>& texture, Crowny::Color color)
			: Texture(texture), Color(color) {}
	};

	struct MeshRendererComponent
	{

	};

	template <>
	void ComponentEditorWidget<TransformComponent>(entt::registry& reg, entt::entity e);

	template <>
	void ComponentEditorWidget<CameraComponent>(entt::registry& reg, entt::entity e);
}