#include "cwpch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Serialization/SpriteSerializer.h"

namespace Crowny
{
    String SpriteSerializer::SerializeToString() const
    {
        if (!m_Sprite)
            return {};
        const auto& data = m_Sprite->GetData();
        YAML::Emitter out;
        out << YAML::BeginMap;
        SerializeValueYAML(out, "Version", SPRITE_FORMAT_VERSION);
        SerializeValueYAML(out, "Name", m_Sprite->GetName());
        SerializeValueYAML(out, "Texture", data.TextureId);
        SerializeValueYAML(out, "UvRect", data.UvRect);
        SerializeValueYAML(out, "Pivot", data.Pivot);
        SerializeValueYAML(out, "OriginalSize", data.OriginalSize);
        SerializeValueYAML(out, "PixelsPerUnit", data.PixelsPerUnit);
        SerializeValueYAML(out, "Borders", data.Borders);
        out << YAML::EndMap;
        return out.c_str();
    }

    bool SpriteSerializer::Serialize(const Path& path) const
    {
        if (!m_Sprite || path.empty())
            return false;
        String error;
        if (FileSystem::WriteTextFileAtomic(path, SerializeToString(), &error))
            return true;
        CW_ENGINE_ERROR("Cannot save sprite '{}': {}", path, error);
        return false;
    }

    bool SpriteSerializer::Deserialize(const Path& path)
    {
        if (!DeserializeFromString(FileSystem::ReadTextFile(path)))
            return false;
        if (m_Sprite->GetName().empty())
            m_Sprite->SetName(path.stem().string());
        return true;
    }

    bool SpriteSerializer::DeserializeFromString(const String& text)
    {
        if (!m_Sprite)
            return false;
        try
        {
            const auto source = YAML::Load(text);
            if (!source.IsMap() || source["Version"].as<uint32_t>(0) != SPRITE_FORMAT_VERSION)
                return false;
            SpriteData data;
            String name;
            const auto read = [&](const char* key, auto& value) {
                // Defaults apply only to absent fields, never to malformed values.
                if (const auto field = source[key])
                    value = field.as<std::remove_cvref_t<decltype(value)>>();
            };
            read("Name", name);
            read("Texture", data.TextureId);
            read("UvRect", data.UvRect);
            read("Pivot", data.Pivot);
            read("OriginalSize", data.OriginalSize);
            read("PixelsPerUnit", data.PixelsPerUnit);
            read("Borders", data.Borders);
            if (!m_Sprite->SetData(data))
                return false;
            m_Sprite->SetName(name);
            return true;
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Cannot read sprite: {}", error.what());
            return false;
        }
    }
} // namespace Crowny
