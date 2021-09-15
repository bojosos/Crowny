#pragma once

#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/IndexBuffer.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/VertexBuffer.h"

namespace Crowny
{

    class ForwardRenderer;

    class Skybox
    {
    public:
        Skybox(const std::string& filepath);
        ~Skybox() = default;

    private:
        void GenerateBRDFLUT();
        void GeneratePrefilteredCube();
        void GenerateIrradianceCube();

    private:
        friend class ForwardRenderer;
        Ref<Texture> m_EnvironmentMap; // the loaded map
        Ref<Texture> m_PrefilteredMap; // the filtered map
        Ref<Texture> m_IrradianceMap;  // the irradiance map
        Ref<Texture> m_Brdf;           // should not be here

        uint32_t m_Width, m_Height, m_Channels;

        Ref<VertexBuffer> m_SkyboxVbo;
        Ref<IndexBuffer> m_SkyboxIbo;
    };

} // namespace Crowny