#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Constants.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Import/MaterialImporter.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/Material.h"

namespace Crowny
{
    bool MaterialImporter::IsExtensionSupported(const String& ext) const
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "mat";
    }

    bool MaterialImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return false; }

    Ref<Asset> MaterialImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        // Create a default Unlit material. The project library imports .mat files
        // as new assets when they are first added; any saved parameters are stored
        // in the UUID-keyed cache and loaded from there afterwards via LoadFromUUID.
        AssetHandle<Shader> shader = gAssetManager->Load<Shader>(UNLIT_SHADER_PATH);
        if (shader)
            return Material::CreateUnlit(shader);

        // Unlit shader not yet compiled — return a bare material with no shader.
        // The user can assign a shader from the inspector.
        return CreateRef<Material>(AssetHandle<Shader>{});
    }
} // namespace Crowny
