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
		glm::vec3 Position;
		uint32_t Color;
		glm::vec2 Uv;
		float Tid;
	};

	class BatchRenderer2D
	{
	public:
		static void Begin(const Camera& camera);

		static float FindTexture(const Ref<Texture2D>& texture);
		
		static void FillRect(const Rectangle& bounds, Color color);
		static void FillRect(const Rectangle& bounds, const Ref<Texture2D>& texture, Color color);

		static void DrawString(const std::string& text, float x, float y, const Ref<Font>& font, Color color);
		static void End();
		static void Flush();
		static void Init();
	};
}