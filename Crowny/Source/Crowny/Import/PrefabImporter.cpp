#include "cwpch.h"

#include "Crowny/Common/StringUtils.h"
#include "Crowny/Import/PrefabImporter.h"
#include "Crowny/Scene/Prefab.h"
#include "Crowny/Serialization/PrefabSerializer.h"

namespace Crowny
{
    bool PrefabImporter::IsExtensionSupported(const String& ext) const
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "cwprefab";
    }

    bool PrefabImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return false; }

    Ref<Asset> PrefabImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        Ref<Prefab> prefab = CreateRef<Prefab>();
        PrefabSerializer serializer(prefab);
        serializer.Deserialize(path);
        return prefab;
    }
} // namespace Crowny
