#include "cwpch.h"

#include "Crowny/Import/MaterialPresetImporter.h"

#include "Crowny/Renderer/MaterialPresetLibrary.h"

namespace Crowny
{
    bool MaterialPresetImporter::IsExtensionSupported(const String& ext) const { return ext == "cwpreset"; }

    bool MaterialPresetImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return false; }

    Ref<Asset> MaterialPresetImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        (void)importOptions;
        Ref<MaterialPreset> preset = MaterialPresetLibrary::LoadFromFile(path);
        if (preset == nullptr)
        {
            CW_ENGINE_ERROR("Failed to import material preset '{}'.", path);
            return nullptr;
        }
        return preset;
    }
} // namespace Crowny
