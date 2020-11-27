#pragma once

#include "Crowny/Common/Uuid.h"
#include "Crowny/Renderer/Texture.h"

namespace Crowny
{
    class AssetManager;

    class AssetManifest
    {
    public:
        void Serialize(const std::string& filepath);
        const std::string& GetName() const { return m_Name; }
        Ref<Texture2D> LoadTexture(const std::string& texture);

    private:
        friend class AssetManager;
        AssetManifest(const std::string& name);
        void Deserialize(const std::string& filepath);

    private:
        std::string& m_Name;
        std::unordered_map<std::string, Uuid> m_Uuids;
        std::unorderd_map<Uuid, std::string> m_Paths;
    }
}