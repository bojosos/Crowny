#include "cwpch.h"

#include "Crowny/Renderer/BuiltInShaderCatalog.h"

#include "Crowny/Application/Application.h"
#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Assets/AssetManifest.h"
#include "Crowny/Common/BuiltInResourcePack.h"
#include "Crowny/Common/Constants.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Hash.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/Renderer/GpuMaterial.h"

#include <algorithm>
#include <array>

namespace Crowny
{
    namespace
    {
        constexpr std::array<const char*, 3> KNOWN_MATERIAL_SHADERS = { UNLIT_SHADER_PATH, TOON_SHADER_PATH, PBRIBL_SHADER_PATH };
        constexpr StringView UUID_NAMESPACE = "crowny-builtin-shader:";

        AssetManager* s_RegisteredManager = nullptr;

        void AddCandidate(Vector<Path>& candidates, const Path& candidate)
        {
            std::error_code error;
            if (candidate.empty() || !fs::is_directory(candidate, error) || error)
                return;
            const Path normalized = candidate.lexically_normal();
            if (std::find(candidates.begin(), candidates.end(), normalized) == candidates.end())
                candidates.push_back(normalized);
        }

        BuiltInShaderEntry MakeEntry(const Path& relativeAssetPath)
        {
            BuiltInShaderEntry entry;
            entry.AssetPath = relativeAssetPath.lexically_normal();
            entry.Name = entry.AssetPath.stem().string();
            entry.Uuid = BuiltInShaderCatalog::MakeStableUuid(entry.AssetPath.generic_string());
            return entry;
        }

        void SortEntries(Vector<BuiltInShaderEntry>& entries)
        {
            std::sort(entries.begin(), entries.end(), [](const BuiltInShaderEntry& left, const BuiltInShaderEntry& right) {
                return left.Name != right.Name ? left.Name < right.Name : left.AssetPath < right.AssetPath;
            });
        }
    } // namespace

    UUID BuiltInShaderCatalog::MakeStableUuid(StringView relativeAssetPath)
    {
        const String seed = String(UUID_NAMESPACE) + String(relativeAssetPath);
        const uint64_t first = Hashing::CityHash64(seed.data(), seed.size());
        const uint64_t second = Hashing::CityHash64WithSeed(seed.data(), seed.size(), first ^ 0x9E3779B97F4A7C15ull);
        uint32_t data[4] = { static_cast<uint32_t>(first >> 32), static_cast<uint32_t>(first), static_cast<uint32_t>(second >> 32),
                             static_cast<uint32_t>(second) };
        // Mark as a name-based (version 8, RFC 9562) UUID so it can never collide with generated version 4 ids.
        data[1] = (data[1] & 0xFFFF0FFFu) | 0x00008000u;
        data[2] = (data[2] & 0x3FFFFFFFu) | 0x80000000u;
        if (data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 0)
            data[3] = 1u;
        return UUID(data[0], data[1], data[2], data[3]);
    }

    Vector<Path> BuiltInShaderCatalog::ResolveShaderDirectories()
    {
        Vector<Path> candidates;
        if (BuiltInResourcePack::IsStartedUp())
            AddCandidate(candidates, BuiltInResourcePack::Get().GetPath().parent_path().parent_path() / SHADER_ASSET_DIR);
        if (Application* application = Application::TryGet())
        {
            AddCandidate(candidates, application->GetWorkingDirectory() / "Crowny-Editor" / SHADER_ASSET_DIR);
            AddCandidate(candidates, application->GetWorkingDirectory() / SHADER_ASSET_DIR);
        }
        std::error_code error;
        const Path current = fs::current_path(error);
        if (!error)
        {
            AddCandidate(candidates, current / SHADER_ASSET_DIR);
            AddCandidate(candidates, current / "Crowny-Editor" / SHADER_ASSET_DIR);
        }
        return candidates;
    }

    Vector<BuiltInShaderEntry> BuiltInShaderCatalog::EnumerateDirectory(const Path& directory)
    {
        Vector<BuiltInShaderEntry> entries;
        std::error_code error;
        if (!fs::is_directory(directory, error) || error)
            return entries;
        for (fs::directory_iterator iter(directory, fs::directory_options::skip_permission_denied, error), end; !error && iter != end;
             iter.increment(error))
        {
            const fs::directory_entry& fileEntry = *iter;
            if (!fileEntry.is_regular_file(error) || error)
            {
                error.clear();
                continue;
            }
            const Path& path = fileEntry.path();
            if (path.extension() != ".asset")
                continue;
            AssetFileHeader header;
            if (!PeekAssetHeader(path, header) || header.Type != AssetType::Shader)
                continue;
            entries.push_back(MakeEntry(Path(SHADER_ASSET_DIR) / path.filename()));
        }
        SortEntries(entries);
        return entries;
    }

    Vector<BuiltInShaderEntry> BuiltInShaderCatalog::Enumerate()
    {
        Vector<BuiltInShaderEntry> entries;
        for (const Path& directory : ResolveShaderDirectories())
        {
            for (BuiltInShaderEntry& entry : EnumerateDirectory(directory))
            {
                if (std::none_of(entries.begin(), entries.end(), [&entry](const BuiltInShaderEntry& existing) { return existing.Uuid == entry.Uuid; }))
                    entries.push_back(std::move(entry));
            }
        }
        // Packed builds have no loose directory but the surface shaders are still reachable through the pack.
        for (const char* known : KNOWN_MATERIAL_SHADERS)
        {
            BuiltInShaderEntry entry = MakeEntry(known);
            if (std::any_of(entries.begin(), entries.end(), [&entry](const BuiltInShaderEntry& existing) { return existing.Uuid == entry.Uuid; }))
                continue;
            if (FileSystem::FileExists(entry.AssetPath))
                entries.push_back(std::move(entry));
        }
        SortEntries(entries);
        return entries;
    }

    Ref<AssetManifest> BuiltInShaderCatalog::BuildManifest(const Vector<BuiltInShaderEntry>& entries)
    {
        Ref<AssetManifest> manifest = CreateRef<AssetManifest>("BuiltInShaders");
        for (const BuiltInShaderEntry& entry : entries)
            manifest->RegisterAsset(entry.Uuid, entry.AssetPath);
        return manifest;
    }

    void BuiltInShaderCatalog::EnsureRegistered()
    {
        AssetManager* assetManager = AssetManager::TryGet();
        if (assetManager == nullptr || s_RegisteredManager == assetManager)
            return;
        const Vector<BuiltInShaderEntry> entries = Enumerate();
        if (entries.empty())
            return;
        assetManager->RegisterAssetManifest(BuildManifest(entries));
        s_RegisteredManager = assetManager;
        CW_ENGINE_INFO("Registered {} built-in shaders with stable identifiers.", entries.size());
    }

    bool BuiltInShaderCatalog::IsMaterialShader(StringView assetPath, const Shader& shader)
    {
        if (shader.GetTechniques().empty())
            return false;
        const Ref<ShaderTechnique>& technique = shader.GetTechniques().front();
        if (technique == nullptr)
            return false;
        const bool hasGraphicsPass = std::any_of(technique->GetRenderPasses().begin(), technique->GetRenderPasses().end(),
                                                 [](const Ref<ShaderRenderPass>& pass) { return pass && !pass->IsCompute() && !pass->IsRayTrace(); });
        if (!hasGraphicsPass && !technique->GetRenderPasses().empty())
            return false;
        const MaterialRenderClassification classification = MaterialRenderClassifier::Classify(assetPath, technique->GetTags(), false, false);
        return !classification.IsUnsupported();
    }
} // namespace Crowny
