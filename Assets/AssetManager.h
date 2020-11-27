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

    public:
        AssetManager(AssetManager const&) = delete;
        void operator=(AssetManager const&) = delete;

        Ref<AssetManifest> ImportManifest(const std::string& path);
        Ref<Texture> ImportTexture(const std::string& path);
        Ref<Shader> ImportShader(const std::string& path);
        Ref<AssetManifest> CreateManifest(const std::string& path);
    }
}