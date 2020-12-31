#pragma once

#include "Crowny/Renderer/VertexArray.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Renderer/Texture.h"
#include "Crowny/Renderer/Camera.h"
#include "Crowny/Common/Color.h"

namespace Crowny
{
	struct VertexData
	{
		glm::vec4 Position;
		glm::vec2 Uv;
		float Tid;
		glm::vec4 Color;
	};

	class Renderer2D
	{
	public:
		static void Init();
		static void Shutdown();

		static float FindTexture(const Ref<Texture2D>& texture);
		
		static void FillRect(const glm::mat4& transform, const Ref<Texture2D>& texture, const glm::vec4& color);
		static void FillRect(const Rectangle& bounds, const glm::vec4& color);
		static void FillRect(const Rectangle& bounds, const Ref<Texture2D>& texture, const glm::vec4& color);

		static void DrawString(const std::string& text, float x, float y, const Ref<Font>& font, const glm::vec4& color);
		static void DrawString(const std::string& text, const glm::mat4& transform, const Ref<Font>& font, const glm::vec4& color);

		static void Begin(const Camera& camera, const glm::mat4& viewMatrix);
		static void End();
		static void Flush();
	};
}