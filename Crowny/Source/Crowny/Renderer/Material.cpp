#include "cwpch.h"

#include "Crowny/Renderer/Material.h"

namespace Crowny
{
    Material::Material(const AssetHandle<Shader>& shader) : m_Shader(shader) {}

    void Material::SetUniformData(const String& name, byte* data) {}

    void Material::SetTexture(const String& name, const Ref<Texture>& texture) {}

    void MaterialInstance::SetUniformData(const String& name, byte* data) {}

    void MaterialInstance::SetTexture(const String& name, const Ref<Texture>& texture) {}

    MaterialInstance::MaterialInstance(const Ref<Material>& material) {}

} // namespace Crowny