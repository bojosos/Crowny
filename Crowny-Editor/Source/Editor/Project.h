#pragma once

#include "Crowny/Assets/AssetManifest.h"

namespace Crowny
{
    class Project
    {
    public:
        Project(const std::filesystem::path& path, const std::string& name);

        void Save();

    private:
        float m_Scale;
        uint32_t m_DefaultSceneIndex;
        uint32_t m_LastSceneOpen;
        std::filesystem::path m_LastOpenDirectory;

        std::filesystem::path m_Directory;
        std::string m_ProjectName;
        Ref<AssetManifest> m_AssetManifest;
    };
} // namespace Crowny