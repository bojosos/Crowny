#include "cwpch.h"

#include "Crowny/Import/MaterialImporter.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Renderer/Material.h"

namespace Crowny
{
    bool MaterialImporter::IsExtensionSupported(const String& ext) const
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "mat";
    }

    bool MaterialImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return false; }

    Ref<Asset> MaterialImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        // .mat files are saved as binary assets by the asset pipeline.
        // Direct import from .mat is not supported — materials are created
        // during mesh import or programmatically. Return nullptr so the
        // asset system falls back to loading from the cached .asset file.
        return nullptr;
    }
} // namespace Crowny
