#pragma once

#include "Crowny/RenderAPI/GraphicsPipeline.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/RenderAPI/VertexBuffer.h"
#include "Crowny/RenderAPI/IndexBuffer.h"

namespace Crowny
{

    class OpenGLEnvironmentMap
    {
    public:
        OpenGLEnvironmentMap(const std::string& filepath);
        ~OpenGLEnvironmentMap() = default;

        void ToCubemap();
        void GenerateBRDFLUT();
        void GeneratePrefilteredCube();
        void GenerateIrradianceCube();

    private:
        Ref<Texture> m_Texture;
        Ref<Texture> m_Cubemap;
        uint32_t m_Width, m_Height, m_Channels;

        Ref<VertexBuffer> m_SkyboxVbo;
        Ref<IndexBuffer> m_SkyboxIbo;
        BufferLayout m_FilterLayout;
        
        Ref<Texture> m_Envmap;

    };

}