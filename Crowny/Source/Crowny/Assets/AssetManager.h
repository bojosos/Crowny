#pragma once

#include "Crowny/Assets/AssetManifest.h"

namespace Crowny
{
    class AssetManager
    {
    public:
        static AssetManager& Get()
        {
            static AssetManager instance;
            return instance;
        }

    private:
        AssetManager() {}
        AssetManager(AssetManager const&);
        void operator=(AssetManager const&);

    private:
        std::unordered_map<std::string, Ref<AssetManifest>> m_Manifests;

    public:

        Ref<AssetManifest>& ImportManifest(const std::string& path, const std::string& name);
        Ref<AssetManifest>& CreateManifest(const std::string& path, const std::string& name);
    };
}