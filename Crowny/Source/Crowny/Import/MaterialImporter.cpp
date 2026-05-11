#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Constants.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Import/MaterialImporter.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Serialization/MaterialSerializer.h"

namespace Crowny
{
    bool MaterialImporter::IsExtensionSupported(const String& ext) const
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "cwmat" || lower == "mat";
    }

    bool MaterialImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return false; }

    Ref<Asset> MaterialImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        String ext = path.extension().string();
        if (!ext.empty() && ext[0] == '.')
            ext = ext.substr(1);
        StringUtils::ToLower(ext);

        if (ext == "cwmat")
        {
            // Load from YAML source file
            const AssetHandle<Shader> fallbackShader = gAssetManager->Load<Shader>(UNLIT_SHADER_PATH);
            const Ref<Material> material = Material::Create(fallbackShader ? fallbackShader : AssetHandle<Shader>{});
            MaterialSerializer serializer(material);
            serializer.Deserialize(path);
            return material;
        }

        // Legacy .mat: create a default Unlit material
        const AssetHandle<Shader> shader = gAssetManager->Load<Shader>(UNLIT_SHADER_PATH);
        if (shader)
            return Material::CreateUnlit(shader);

        return CreateRef<Material>(AssetHandle<Shader>{});
    }
} // namespace Crowny
