#pragma once

#include "Crowny/Common/Uuid.h"

#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"

namespace Crowny
{
    class AssetManager;

    class AssetManifest
    {
    public:
        AssetManifest(const String& name);
        ~AssetManifest() = default;

        const UUID& UuidFromFilepath(const Path& path);
        const Path& FilepathFromUuid(const UUID& uuid);

    private:
        friend class AssetManager;

    private:
        String m_Name;
        UnorderedMap<Path, UUID, HashPath> m_UUIDs;
        UnorderedMap<UUID, Path> m_Paths;
    };

} // namespace Crowny
