#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"

namespace Crowny
{

    Ref<AssetManifest>& AssetManager::CreateManifest(const std::string& filepath, const std::string& name)
    {
        CW_ENGINE_ASSERT(false, "Not implemented");
        // return nullptr;
    }

    Ref<AssetManifest>& AssetManager::ImportManifest(const std::string& filepath, const std::string& name)
    {
        auto it = m_Manifests.find(filepath);
        if (it == m_Manifests.end())
        {
            Ref<AssetManifest> man = CreateRef<AssetManifest>(name);
            m_Manifests[filepath] = man;
            return m_Manifests[filepath];
        }
        else
            return it->second;
    }

} // namespace Crowny
