#include "cwpch.h"

#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Constants.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Import/MaterialImporter.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/BuiltInShaderCatalog.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Serialization/MaterialSerializer.h"

namespace Crowny
{
    bool MaterialImporter::IsExtensionSupported(const String& ext) const { return ext == "cwmat" || ext == "mat"; }

    bool MaterialImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return false; }

    Ref<Asset> MaterialImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        BuiltInShaderCatalog::EnsureRegistered();
        AssetFileHeader header;
        if (PeekAssetHeader(path, header) && header.Type == AssetType::Material)
        {
            const AssetHandle<Material> material = AssetManager::Get().Load<Material>(path);
            return material.GetInternalPtr();
        }
        const Ref<DataStream> stream = FileSystem::OpenFile(path);
        if (!stream)
            return nullptr;
        const String contents = stream->GetAsString();
        stream->Close();
        const Ref<Material> material = Material::Create({});
        if (!MaterialSerializer(material).DeserializeFromString(contents))
            return nullptr;
        if (!material->GetShader())
        {
            const Ref<Material> fallback = Material::CreateDefault();
            if (!fallback)
                return nullptr;
            material->SetShader(fallback->GetShader());
            material->ApplyModelDefaults();
            if (!MaterialSerializer(material).DeserializeFromString(contents))
                return nullptr;
        }
        return material;
    }
} // namespace Crowny
