#pragma once

#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/Texture.h"

#include <glm/glm.hpp>

namespace Crowny
{
	class PBRMaterial : public Material 
	{
	private:
	public:
		PBRMaterial(const Ref<Shader>& shader);
		~PBRMaterial() = default;

		void SetEnvironmentMap(); // cube here

		void SetAlbedo(const glm::vec4& color);
		void SetMetalness(float value);
		void SetRoughness(float value);

		void SetAlbedoMap(const Ref<Texture2D>& texture);
		void SetMetalnessMap(const Ref<Texture2D>& texture);
		void SetRoughnessMap(const Ref<Texture2D>& texture);
		void SetNormalMap(const Ref<Texture2D>& texture);
		void SetAoMap(const Ref<Texture2D>& texture);

		Ref<Texture2D> GetAlbedoMap();
		Ref<Texture2D> GetMetalnessMap();
		Ref<Texture2D> GetNormalMap();
		Ref<Texture2D> GetRoughnessMap();
		Ref<Texture2D> GetAoMap();

	};
}