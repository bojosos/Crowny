#pragma once

#include "Editor/Settings/EditorSettings.h"

namespace Crowny
{
    struct EditorSettingsPaths
    {
        Path PersistentPath;
        Path LoadPath;
        bool MigrateLegacy = false;
    };

    namespace EditorSettingsPersistenceDetail
    {
        inline String ProjectPathKey(const Path& path)
        {
            String key = path.lexically_normal().generic_string();
#ifdef _WIN32
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
#endif
            return key;
        }
    } // namespace EditorSettingsPersistenceDetail

    inline Path NormalizeProjectPath(const Path& projectPath) { return projectPath.lexically_normal(); }

    template <typename CanonicalizePath> bool NormalizeRecentProjects(EditorSettings& settings, CanonicalizePath&& canonicalizePath)
    {
        Array<RecentProject, 5> normalized{};
        size_t nextIndex = 0;
        for (const RecentProject& recent : settings.RecentProjects)
        {
            if (recent.ProjectPath.empty())
                continue;
            const Path path = NormalizeProjectPath(canonicalizePath(recent.ProjectPath));
            if (path.empty())
                continue;

            const String pathKey = EditorSettingsPersistenceDetail::ProjectPathKey(path);
            bool duplicate = false;
            for (size_t index = 0; index < nextIndex; ++index)
            {
                if (EditorSettingsPersistenceDetail::ProjectPathKey(normalized[index].ProjectPath) == pathKey)
                {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate)
                continue;

            normalized[nextIndex++] = { path, recent.Timestamp };
            if (nextIndex == normalized.size())
                break;
        }

        const Path normalizedLastOpen =
          settings.LastOpenProject.empty() ? Path{} : NormalizeProjectPath(canonicalizePath(settings.LastOpenProject));
        bool changed = settings.LastOpenProject != normalizedLastOpen;
        for (size_t index = 0; index < normalized.size() && !changed; ++index)
        {
            changed = settings.RecentProjects[index].ProjectPath != normalized[index].ProjectPath ||
                      settings.RecentProjects[index].Timestamp != normalized[index].Timestamp;
        }

        settings.RecentProjects = std::move(normalized);
        settings.LastOpenProject = normalizedLastOpen;
        return changed;
    }

    inline void RecordRecentProject(EditorSettings& settings, const Path& projectPath, std::time_t timestamp)
    {
        const Path normalizedPath = NormalizeProjectPath(projectPath);
        if (normalizedPath.empty())
            return;

        Array<RecentProject, 5> reordered{};
        reordered[0] = { normalizedPath, timestamp };
        size_t nextIndex = 1;
        const String recordedKey = EditorSettingsPersistenceDetail::ProjectPathKey(normalizedPath);

        for (const RecentProject& recent : settings.RecentProjects)
        {
            if (recent.ProjectPath.empty() || EditorSettingsPersistenceDetail::ProjectPathKey(recent.ProjectPath) == recordedKey)
                continue;
            if (nextIndex == reordered.size())
                break;
            reordered[nextIndex++] = recent;
        }

        settings.RecentProjects = std::move(reordered);
        settings.LastOpenProject = normalizedPath;
    }

    template <typename PathExists> Path SelectStartupProject(const EditorSettings& settings, PathExists&& pathExists)
    {
        for (const RecentProject& recent : settings.RecentProjects)
        {
            if (!recent.ProjectPath.empty() && pathExists(recent.ProjectPath))
                return recent.ProjectPath;
        }

        if (!settings.LastOpenProject.empty() && pathExists(settings.LastOpenProject))
            return settings.LastOpenProject;
        return {};
    }

    inline EditorSettingsPaths SelectEditorSettingsPaths(const Path& persistentPath, const Path& legacyPath, bool persistentExists,
                                                          bool legacyExists)
    {
        if (persistentExists)
            return { persistentPath, persistentPath, false };
        if (legacyExists)
            return { persistentPath, legacyPath, true };
        return { persistentPath, {}, false };
    }
} // namespace Crowny
