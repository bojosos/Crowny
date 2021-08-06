/*#pragma once

#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{

    class OpenGLEnvironmentMap : public EnvironmentMap
    {
    public:
        OpenGLEnvironmentMap(const std::string& filepath);
        ~OpenGLEnvironmentMap() = default;

        virtual uint32_t GetWidth() const override { return m_Width; }
        virtual uint32_t GetHeight() const override { return m_Height; }
        virtual uint32_t GetRendererID() const override { return m_RendererID; }

        virtual void Clear(int32_t clearColor) override;
        virtual void SetData(void* data, uint32_t size) override {};
        virtual void SetData(void* data, TextureChannel channel) override {};

        void ToCubemap();

        virtual void Bind(uint32_t slot) const override;
        virtual void BindSkybox(uint32_t slot) const override;
        virtual void Unbind(uint32_t slot) const override;
        virtual bool operator==(const Texture& other) const override { return m_RendererID == other.GetRendererID(); };

        virtual const std::string& GetFilepath() const override { return ""; }
        virtual const std::string& GetName() const override { return m_Name; }

    private:
        uint32_t m_RendererID;
        uint32_t m_Width, m_Height, m_Channels;
        uint32_t m_Cubemap, m_IrradianceMap;
        uint32_t m_BrdfLUTTexture;
        uint32_t m_PrefilterMap;
        std::string m_Name;

    };

}*/