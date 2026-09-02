#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Uuid.h"

namespace Crowny
{
    class AssetManifest;
    class Shader;

    struct BuiltInShaderEntry
    {
        String Name;    // Display name, the asset stem ("Unlit")
        Path AssetPath; // Engine-relative path ("Resources/Shaders/Unlit.asset")
        UUID Uuid;      // Stable identifier derived from AssetPath
    };

    /**
     * Enumerates the engine's built-in compiled shaders and gives each a stable
     * UUID so materials can reference them across editor sessions. Built-in
     * shaders are not part of any project manifest; without this catalog a
     * material saved with a built-in shader would store a per-session UUID that
     * cannot be resolved after a restart.
     */
    class BuiltInShaderCatalog
    {
    public:
        static constexpr const char* SHADER_ASSET_DIR = "Resources/Shaders";

        /** Deterministic UUID for an engine-relative asset path (generic separators, case sensitive). */
        static UUID MakeStableUuid(StringView relativeAssetPath);

        /** Shader assets shipped with the engine, sorted by name. */
        static Vector<BuiltInShaderEntry> Enumerate();
        /** Shader assets in `directory`; entries are named relative to SHADER_ASSET_DIR. */
        static Vector<BuiltInShaderEntry> EnumerateDirectory(const Path& directory);
        static Vector<Path> ResolveShaderDirectories();

        static Ref<AssetManifest> BuildManifest(const Vector<BuiltInShaderEntry>& entries);

        /** Registers the built-in manifest with the asset manager once per asset manager lifetime. */
        static void EnsureRegistered();

        /** True when a material using this shader can be rendered (explicit material_model tag or a known built-in surface shader). */
        static bool IsMaterialShader(StringView assetPath, const Shader& shader);
    };
} // namespace Crowny
