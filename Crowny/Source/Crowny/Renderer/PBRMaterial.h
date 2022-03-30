#pragma once

#include "Crowny/Assets/AssetHandle.h"

#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/UniformBufferBlock.h"
#include "Crowny/RenderAPI/UniformParams.h"
#include "Crowny/Renderer/Material.h"

#include <glm/glm.hpp>

namespace Crowny
{
    class PBRMaterial : public Material
    {
    public:
        PBRMaterial(const AssetHandle<Shader>& shader);
        ~PBRMaterial() = default;

        void Bind();

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

        Ref<UniformParams>& GetUniformParams() { return m_Uniforms; }

    private:
        struct Params
        {
            glm::vec4 Albedo = glm::vec4(1.0f);
            float Roughness = 0.0f;
            float Metalness = 0.0f;
        };

        bool m_HasChanged = false;
        Params m_Params;
        Ref<UniformBufferBlock> m_MaterialParams;
        Ref<UniformParams> m_Uniforms;
        Ref<GraphicsPipeline> m_Pipeline;
    };
} // namespace Crowny