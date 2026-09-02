#include "cwpch.h"

#include "Crowny/Renderer/MaterialPresetLibrary.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Common/BuiltInResourcePack.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Serialization/MaterialPresetSerializer.h"

#include <algorithm>
#include <array>

namespace Crowny
{
    namespace
    {
        constexpr std::array<const char*, 3> KNOWN_BUILT_IN_PRESETS = { "Toon/Classic", "Toon/Soft", "Toon/Hatched" };

        UnorderedMap<String, Ref<MaterialPreset>>& BuiltInCache()
        {
            static UnorderedMap<String, Ref<MaterialPreset>> s_Cache;
            return s_Cache;
        }

        void AddCandidate(Vector<Path>& candidates, const Path& candidate)
        {
            std::error_code error;
            if (candidate.empty() || !fs::is_directory(candidate, error) || error)
                return;
            const Path normalized = candidate.lexically_normal();
            if (std::find(candidates.begin(), candidates.end(), normalized) == candidates.end())
                candidates.push_back(normalized);
        }
    } // namespace

    String MaterialPresetLibrary::BuiltInToonPresetName(ToonMaterialPreset preset)
    {
        switch (preset)
        {
        case ToonMaterialPreset::Classic:
            return "Toon/Classic";
        case ToonMaterialPreset::Soft:
            return "Toon/Soft";
        case ToonMaterialPreset::Hatched:
            return "Toon/Hatched";
        }
        return {};
    }

    Path MaterialPresetLibrary::BuiltInPresetPath(StringView name) { return Path(PRESET_DIR) / (String(name) + PRESET_EXTENSION); }

    Vector<Path> MaterialPresetLibrary::ResolveBuiltInDirectories()
    {
        Vector<Path> candidates;
        if (BuiltInResourcePack::IsStartedUp())
            AddCandidate(candidates, BuiltInResourcePack::Get().GetPath().parent_path().parent_path() / PRESET_DIR);
        if (Application* application = Application::TryGet())
        {
            AddCandidate(candidates, application->GetWorkingDirectory() / "Crowny-Editor" / PRESET_DIR);
            AddCandidate(candidates, application->GetWorkingDirectory() / PRESET_DIR);
        }
        std::error_code error;
        Path directory = fs::current_path(error);
        for (uint32_t depth = 0; depth < 5 && !error && !directory.empty(); depth++)
        {
            AddCandidate(candidates, directory / PRESET_DIR);
            AddCandidate(candidates, directory / "Crowny-Editor" / PRESET_DIR);
            if (!directory.has_parent_path() || directory.parent_path() == directory)
                break;
            directory = directory.parent_path();
        }
        return candidates;
    }

    Ref<MaterialPreset> MaterialPresetLibrary::LoadFromFile(const Path& path)
    {
        Ref<MaterialPreset> preset = CreateRef<MaterialPreset>();
        MaterialPresetSerializer serializer(preset);
        if (!serializer.Deserialize(path))
            return nullptr;
        return preset;
    }

    Ref<MaterialPreset> MaterialPresetLibrary::LoadBuiltIn(StringView name)
    {
        if (name.empty())
            return nullptr;
        auto& cache = BuiltInCache();
        const auto cached = cache.find(String(name));
        if (cached != cache.end())
            return cached->second;

        Ref<MaterialPreset> preset;
        const Path relativePath = BuiltInPresetPath(name);
        // The relative form resolves through the built-in resource pack when it is mounted.
        if (FileSystem::FileExists(relativePath))
            preset = LoadFromFile(relativePath);
        if (preset == nullptr)
        {
            for (const Path& directory : ResolveBuiltInDirectories())
            {
                const Path candidate = directory / (String(name) + PRESET_EXTENSION);
                std::error_code error;
                if (fs::is_regular_file(candidate, error) && !error)
                {
                    preset = LoadFromFile(candidate);
                    if (preset != nullptr)
                        break;
                }
            }
        }
        if (preset == nullptr)
            CW_ENGINE_WARN("Built-in material preset '{}' was not found.", name);
        cache.emplace(String(name), preset);
        return preset;
    }

    void MaterialPresetLibrary::ClearCache() { BuiltInCache().clear(); }

    Vector<MaterialPresetEntry> MaterialPresetLibrary::EnumerateDirectory(const Path& directory)
    {
        Vector<MaterialPresetEntry> entries;
        std::error_code error;
        if (!fs::is_directory(directory, error) || error)
            return entries;
        for (fs::recursive_directory_iterator iter(directory, fs::directory_options::skip_permission_denied, error), end;
             !error && iter != end; iter.increment(error))
        {
            const fs::directory_entry& fileEntry = *iter;
            if (!fileEntry.is_regular_file(error) || error)
            {
                error.clear();
                continue;
            }
            const Path& path = fileEntry.path();
            if (path.extension() != PRESET_EXTENSION)
                continue;
            MaterialPresetEntry entry;
            entry.Name = path.lexically_relative(directory).replace_extension().generic_string();
            entry.SourcePath = path;
            entry.BuiltIn = true;
            entries.push_back(std::move(entry));
        }
        std::sort(entries.begin(), entries.end(), [](const MaterialPresetEntry& left, const MaterialPresetEntry& right) { return left.Name < right.Name; });
        return entries;
    }

    Vector<MaterialPresetEntry> MaterialPresetLibrary::EnumerateBuiltIn()
    {
        Vector<MaterialPresetEntry> entries;
        for (const Path& directory : ResolveBuiltInDirectories())
        {
            for (MaterialPresetEntry& entry : EnumerateDirectory(directory))
            {
                if (std::none_of(entries.begin(), entries.end(), [&entry](const MaterialPresetEntry& existing) { return existing.Name == entry.Name; }))
                    entries.push_back(std::move(entry));
            }
        }
        // Packed builds may have no loose directory; the shipped presets are still addressable by name.
        for (const char* known : KNOWN_BUILT_IN_PRESETS)
        {
            if (std::any_of(entries.begin(), entries.end(), [known](const MaterialPresetEntry& existing) { return existing.Name == known; }))
                continue;
            const Path relativePath = BuiltInPresetPath(known);
            if (!FileSystem::FileExists(relativePath))
                continue;
            MaterialPresetEntry entry;
            entry.Name = known;
            entry.SourcePath = relativePath;
            entry.BuiltIn = true;
            entries.push_back(std::move(entry));
        }
        std::sort(entries.begin(), entries.end(), [](const MaterialPresetEntry& left, const MaterialPresetEntry& right) { return left.Name < right.Name; });
        return entries;
    }

    Vector<MaterialPresetEntry> MaterialPresetLibrary::FilterCompatible(const Material& material, Vector<MaterialPresetEntry> entries)
    {
        Vector<MaterialPresetEntry> compatible;
        compatible.reserve(entries.size());
        for (MaterialPresetEntry& entry : entries)
        {
            if (entry.Preset == nullptr)
                entry.Preset = entry.BuiltIn ? LoadBuiltIn(entry.Name) : LoadFromFile(entry.SourcePath);
            if (entry.Preset == nullptr || !entry.Preset->IsCompatibleWith(material))
                continue;
            compatible.push_back(std::move(entry));
        }
        return compatible;
    }
} // namespace Crowny
