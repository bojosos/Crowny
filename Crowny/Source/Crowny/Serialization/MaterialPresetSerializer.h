#pragma once

#include "Crowny/Renderer/MaterialPreset.h"

namespace Crowny
{
    /** YAML (.cwpreset) reader and writer for material presets. */
    class MaterialPresetSerializer
    {
    public:
        static constexpr uint32_t YAML_VERSION = 1;

        explicit MaterialPresetSerializer(const Ref<MaterialPreset>& preset);

        bool Serialize(const Path& filepath);
        bool Deserialize(const Path& filepath);

        String SerializeToString() const;
        bool DeserializeFromString(const String& yamlString);

    private:
        Ref<MaterialPreset> m_Preset;
    };
} // namespace Crowny
