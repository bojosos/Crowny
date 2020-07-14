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
		void SetSpecular(const glm::vec3& color);
		void SetGloss(float value);
		void UsingNormalMap(bool value);

		void SetAlbedoMap(const Ref<Texture2D>& texture);
		void SetSpecularMap(const Ref<Texture2D>& texture);
		void SetGlossMap(const Ref<Texture2D>& texture);
		void SetNormalMap(const Ref<Texture2D>& texture);

		Ref<Texture2D> GetAlbedoMap();
		Ref<Texture2D> GetSpecularMap();
		Ref<Texture2D> GetNormalMap();
		Ref<Texture2D> GetGlossMap();

	};
}