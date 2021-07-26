#pragma once

#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/Texture.h"

#include <glm/glm.hpp>

namespace Crowny
{
	// VULKAN IMPL: Fix
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

		void SetAlbedoMap(const Ref<Texture>& texture);
		void SetMetalnessMap(const Ref<Texture>& texture);
		void SetRoughnessMap(const Ref<Texture>& texture);
		void SetNormalMap(const Ref<Texture>& texture);
		void SetAoMap(const Ref<Texture>& texture);

		Ref<Texture> GetAlbedoMap();
		Ref<Texture> GetMetalnessMap();
		Ref<Texture> GetNormalMap();
		Ref<Texture> GetRoughnessMap();
		Ref<Texture> GetAoMap();

	};
}