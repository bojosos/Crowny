#pragma once

#include "Crowny/Common/Uuid.h"

#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{
    class AssetManager;
    class AssetManifestSerializer;

    class ProjectLibrary; // Editor class

    class AssetManifest
    {
    public:
        AssetManifest() = default;
        AssetManifest(const String& name);

        bool UuidToFilepath(const UUID42& uuid, Path& outPath) const;
        bool FilepathToUuid(const Path& path, UUID42& outUuid) const;
        bool UuidExists(const UUID42& uuid) const;
        bool FilepathExists(const Path& path) const;

        void RegisterAsset(const UUID42& uuid, const Path& path);
        void UnregisterAsset(const UUID42& uuid);

        static void Serialize(const Ref<AssetManifest>& manifest, const Path& filepath, const Path& relativeTo);
        static Ref<AssetManifest> Deserialize(const Path& filepath, const Path& relativeTo);

        const String& GetName() const { return m_Name; }
        using Serializer = AssetManifestSerializer;

    private:
        friend class AssetManager;
        friend class AssetManifestSerializer;

        friend class ProjectLibrary;

        String m_Name;
        UnorderedMap<UUID42, Path> m_UuidToFilepath;
        UnorderedMap<Path, UUID42, HashPath> m_FilepathToUuid;
    };

} // namespace Crowny
