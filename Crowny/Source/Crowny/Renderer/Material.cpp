#include "cwpch.h"

#include "Crowny/Renderer/Material.h"

namespace Crowny
{
    Material::Material(const Ref<Shader>& shader) : m_Shader(shader) {}

    void Material::SetUniformData(const std::string& name, byte* data) {}

    void Material::SetTexture(const std::string& name, const Ref<Texture>& texture) {}

    void MaterialInstance::SetUniformData(const std::string& name, byte* data) {}

    void MaterialInstance::SetTexture(const std::string& name, const Ref<Texture>& texture) {}

    MaterialInstance::MaterialInstance(const Ref<Material>& material) {}

} // namespace Crowny