#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Renderer/MaterialPreset.h"

namespace Crowny
{
    struct MaterialPresetEntry
    {
        String Name;      // "Toon/Classic" for built-ins, the asset name for project presets
        Path SourcePath;  // .cwpreset source (relative built-in path or absolute project path)
        UUID Uuid;        // Project presets only
        bool BuiltIn = false;
        Ref<MaterialPreset> Preset; // Loaded lazily for built-ins
    };

    /**
     * Discovers material presets. Built-in presets live in Resources/Presets
     * (loose files while developing, Builtin.cwpack when shipped) and are
     * addressed by their directory-relative name such as "Toon/Classic".
     */
    class MaterialPresetLibrary
    {
    public:
        static constexpr const char* PRESET_DIR = "Resources/Presets";
        static constexpr const char* PRESET_EXTENSION = ".cwpreset";

        static String BuiltInToonPresetName(ToonMaterialPreset preset);
        static Path BuiltInPresetPath(StringView name);

        /** Loads (and caches) a built-in preset by name. Returns null when the data file is missing or invalid. */
        static Ref<MaterialPreset> LoadBuiltIn(StringView name);
        static Ref<MaterialPreset> LoadFromFile(const Path& path);
        static void ClearCache();

        /** Built-in presets found on disk or in the pack, sorted by name. */
        static Vector<MaterialPresetEntry> EnumerateBuiltIn();
        static Vector<MaterialPresetEntry> EnumerateDirectory(const Path& directory);
        static Vector<Path> ResolveBuiltInDirectories();

        /** Keeps entries whose preset targets the material's model or shader. Built-in presets are loaded on demand. */
        static Vector<MaterialPresetEntry> FilterCompatible(const Material& material, Vector<MaterialPresetEntry> entries);
    };
} // namespace Crowny
