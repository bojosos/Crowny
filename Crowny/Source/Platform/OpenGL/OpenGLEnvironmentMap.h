/*#pragma once

#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{

    class OpenGLEnvironmentMap : public EnvironmentMap
    {
    public:
        OpenGLEnvironmentMap(const std::string& filepath);
        ~OpenGLEnvironmentMap() = default;

        void ToCubemap();

    private:
        uint32_t m_RendererID;
        uint32_t m_Width, m_Height, m_Channels;
        uint32_t m_Cubemap, m_IrradianceMap;
        uint32_t m_BrdfLUTTexture;
        uint32_t m_PrefilterMap;
        std::string m_Name;

    };

}*/