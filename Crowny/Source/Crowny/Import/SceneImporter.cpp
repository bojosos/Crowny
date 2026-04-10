#include "cwpch.h"

#include "Crowny/Import/SceneImporter.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/Scene/Scene.h"

namespace Crowny
{
    bool SceneImporter::IsExtensionSupported(const String& ext) const
    {
        String lower = ext;
        StringUtils::ToLower(lower);
        return lower == "cwscene";
    }

    bool SceneImporter::IsMagicNumSupported(uint8_t* num, uint32_t numSize) const { return false; }

    Ref<Asset> SceneImporter::Import(const Path& path, Ref<const ImportOptions> importOptions)
    {
        // Don't fully deserialize the scene during import — script classes may not be loaded,
        // and the physics world may not exist. Just create a lightweight Scene asset that
        // records the file path. Full deserialization happens when the editor opens the scene.
        Ref<Scene> scene = CreateRef<Scene>(path.stem().string(), false);
        return scene;
    }
} // namespace Crowny
