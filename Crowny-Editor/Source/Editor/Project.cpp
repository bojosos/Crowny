#include "cwpch.h"

#include "Editor/Project.h"

namespace Crowny
{

    Project::Project(const std::filesystem::path& path, const std::string& name)
    {
        // std::filesystem::path pathTmp = path / name;
        // if (!std::filesystem::exists(pathTmp))
        // {
        //     std::filesystem::create_directory(pathTmp);
        // }
        // if (!std::filesystem::exists(pathTmp / "Assets"))
        //     std::filesystem::create_directory(path / "Assets");

        // if (std::filesystem::exists(path))
        //     m_AssetManifest = AssetManifest::Load("AssetManifest.cwm", path);
        // else
        //     m_AssetManifest = AssetManifest::Create("AssetManifest.cwm", path);
        // AssetManager::Get().RegisterManifest()
    }

    void Project::Save() {}

    // void Project::Import()
    // {

    // }
} // namespace Crowny