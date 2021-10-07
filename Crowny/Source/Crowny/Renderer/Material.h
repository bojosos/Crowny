#pragma once

#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{
    class Material
    {
    public:
        Material(const Ref<Shader>& shader);
        ~Material() = default;

        // void Bind(uint32_t startslot);

        void SetUniformData(const String& name, byte* data);
        void SetTexture(const String& name, const Ref<Texture>& texture);

        Ref<Shader> GetShader() { return m_Shader; }

    protected:
        Vector<Ref<Texture>> m_Textures;
        Ref<Shader> m_Shader;

    private:
        friend class MaterialInstance;
    };

    class MaterialInstance
    {
    public:
        MaterialInstance(const Ref<Material>& material);
        const Ref<Material>& GetMaterial() const { return m_Material; }

        // void Bind(uint32_t startslot);
        void SetUniformData(const String& name, byte* data);
        void SetTexture(const String& name, const Ref<Texture>& texture);

    private:
        Vector<Ref<Texture>> m_Textures;
        Ref<Material> m_Material;
    };
} // namespace Crowny
