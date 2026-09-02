#pragma once

#include "Crowny/Assets/Asset.h"
#include "Crowny/Common/Uuid.h"
#include "Editor/AssetLibraryTypes.h"

#include <algorithm>
#include <functional>

namespace Crowny::UI
{
    // One selectable row of the asset reference picker.
    struct AssetSearchCandidate
    {
        UUID Uuid;
        String Name;
        bool Subasset = false; // Imported as part of another source file (e.g. a material inside an .obj).
    };

    // Human readable name for an asset that was imported as part of `fileName`.
    // Sub-asset keys have the shape "<type>:<name>#<occurrence>"; the type prefix is implied by the
    // picker's filter and the first occurrence suffix is noise, so both are dropped.
    inline String FormatSubassetName(const String& fileName, const AssetMetadata& metadata, size_t fallbackIndex)
    {
        String key = metadata.SubassetKey;
        const size_t typeSeparator = key.find(':');
        if (typeSeparator != String::npos)
            key.erase(0, typeSeparator + 1);
        const size_t occurrence = key.rfind('#');
        if (occurrence != String::npos && key.compare(occurrence, String::npos, "#0") == 0)
            key.erase(occurrence);
        if (key.empty())
            key = std::to_string(fallbackIndex + 1);
        return fileName + " / " + key;
    }

    // Placeholder shown in the picker's search box for a given asset filter.
    inline const char* GetAssetSearchHint(AssetType type)
    {
        switch (type)
        {
        case AssetType::AudioClip:
            return "Search audio clips...";
        case AssetType::Texture:
            return "Search textures...";
        case AssetType::Shader:
            return "Search shaders...";
        case AssetType::Material:
            return "Search materials...";
        case AssetType::Mesh:
        case AssetType::MeshSource:
            return "Search meshes...";
        case AssetType::ScriptCode:
            return "Search scripts...";
        case AssetType::PhysicsMaterial2D:
        case AssetType::PhysicsMaterial:
            return "Search physics materials...";
        case AssetType::PhysicsMesh:
            return "Search physics meshes...";
        case AssetType::Font:
            return "Search fonts...";
        case AssetType::Scene:
            return "Search scenes...";
        case AssetType::NodeGraph:
            return "Search node graphs...";
        case AssetType::EnvironmentMap:
            return "Search environment maps...";
        case AssetType::Prefab:
            return "Search prefabs...";
        case AssetType::AudioMixer:
            return "Search audio mixers...";
        case AssetType::AnimationClip:
            return "Search animation clips...";
        default:
            return "Search assets...";
        }
    }

    // Collects every asset of `type` reachable from `root`, once per UUID, sorted by display name.
    // Unlike the UUID->path index, the per-file metadata is consulted so dependents imported from a
    // source file (materials/textures of a mesh, clips of an FBX, ...) are typed and named individually
    // instead of all showing up as the source file. `isAvailable` may reject UUIDs that have no
    // imported output yet.
    inline Vector<AssetSearchCandidate> CollectAssetSearchCandidates(const DirectoryEntry* root, AssetType type,
                                                                     const std::function<bool(const UUID&)>& isAvailable = nullptr)
    {
        Vector<AssetSearchCandidate> result;
        if (root == nullptr)
            return result;

        UnorderedSet<UUID> seen;
        auto add = [&](const UUID& uuid, String name, bool subasset) {
            if (uuid.Empty() || !seen.insert(uuid).second)
                return;
            if (isAvailable && !isAvailable(uuid))
                return;
            result.push_back({ uuid, std::move(name), subasset });
        };

        Stack<const DirectoryEntry*> pending;
        pending.push(root);
        while (!pending.empty())
        {
            const DirectoryEntry* directory = pending.top();
            pending.pop();
            for (const Ref<LibraryEntry>& child : directory->Children)
            {
                if (child == nullptr)
                    continue;
                if (child->Type == LibraryEntryType::Directory)
                {
                    pending.push(static_cast<const DirectoryEntry*>(child.get()));
                    continue;
                }
                const FileEntry* file = static_cast<const FileEntry*>(child.get());
                if (file->Metadata != nullptr && file->Metadata->Type == type)
                    add(file->Metadata->Uuid, file->ElementName, false);
                for (size_t i = 0; i < file->DependentMetadata.size(); i++)
                {
                    const Ref<AssetMetadata>& dependent = file->DependentMetadata[i];
                    if (dependent != nullptr && dependent->Type == type)
                        add(dependent->Uuid, FormatSubassetName(file->ElementName, *dependent, i), true);
                }
            }
        }

        std::sort(result.begin(), result.end(), [](const AssetSearchCandidate& a, const AssetSearchCandidate& b) {
            String left = a.Name;
            String right = b.Name;
            StringUtils::ToLower(left);
            StringUtils::ToLower(right);
            if (left != right)
                return left < right;
            return a.Uuid.ToString() < b.Uuid.ToString();
        });
        return result;
    }
} // namespace Crowny::UI
