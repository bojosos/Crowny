#pragma once

#include "Crowny/Common/Uuid.h"

#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{
    class AssetManager;
    class AssetManifestSerializer;

    class AssetManifest
    {
    public:
        AssetManifest() = default;
        AssetManifest(const String& name);

        bool UuidToFilepath(const UUID& uuid, Path& outPath) const;
        bool FilepathToUuid(const Path& path, UUID& outUuid) const;
        bool UuidExists(const UUID& uuid) const;
        bool FilepathExists(const Path& path) const;

        void RegisterAsset(const UUID& uuid, const Path& path);
        void UnregisterAsset(const UUID& uuid);

        static void Serialize(const Ref<AssetManifest>& manifest, const Path& filepath, const Path& relativeTo);
        static Ref<AssetManifest> Deserialize(const Path& filepath, const Path& relativeTo);

        const String& GetName() const { return m_Name; }
        using Serializer = AssetManifestSerializer;
    private:
        friend class AssetManager;
        friend class AssetManifestSerializer;

        String m_Name;
        UnorderedMap<UUID, Path> m_UuidToFilepath;
        UnorderedMap<Path, UUID, HashPath> m_FilepathToUuid;
    };

} // namespace Crowny
