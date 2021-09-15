/*#pragma once

#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{
    class OpenGLTextureCube : public TextureCube
    {
    private:
        uint32_t m_RendererID;
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_Bits;
        TextureParameters m_Parameters;
        std::array<std::string, 6> m_Files;
        std::string m_Name;

    public:
        OpenGLTextureCube(const std::string& filepath, const TextureParameters& parameters);
        OpenGLTextureCube(const std::array<std::string, 6>& files, const TextureParameters& parameters);
        OpenGLTextureCube(const std::array<std::string, 6>& filepath, uint32_t mips, InputFormat format, const
TextureParameters& parameters); ~OpenGLTextureCube();

        virtual void SetData(void* data, uint32_t size) override { CW_ENGINE_ASSERT(false); };
        virtual void SetData(void* data, TextureChannel channel) override { CW_ENGINE_ASSERT(false); };
        virtual bool operator==(const Texture& other) const override { return m_RendererID == other.GetRendererID(); };

        virtual uint32_t GetRendererID() const override { return m_RendererID; }
        virtual uint32_t GetWidth() const override { return m_Width; }
        virtual uint32_t GetHeight() const override { return m_Height; }

        //virtual const std::string& GetFilepath() override { return m_Files[0]; }
        virtual const std::string& GetName() const override { return m_Name; }

        virtual void Bind(uint32_t slot = 0) const override;
        virtual void Unbind(uint32_t slot = 0) const override;

    private:
        void LoadFromFile();
        //void LoadFromFiles();
        //void LoadFromVCross(uint32_t mips);
    };
}
*/