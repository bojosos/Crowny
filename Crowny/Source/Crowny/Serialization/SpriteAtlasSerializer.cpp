#include "cwpch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Serialization/SpriteAtlasSerializer.h"

namespace Crowny
{
    String SpriteAtlasSerializer::SerializeToString() const
    {
        if (!m_Atlas)
            return {};
        YAML::Emitter out;
        out << YAML::BeginMap;
        SerializeValueYAML(out, "Version", SPRITE_ATLAS_FORMAT_VERSION);
        SerializeValueYAML(out, "Name", m_Atlas->GetName());
        SerializeValueYAML(out, "PageSize", m_Atlas->GetData().PageSize);
        SerializeValueYAML(out, "MipLevels", m_Atlas->GetData().MipLevels);
        out << YAML::Key << "Sprites" << YAML::Value << YAML::BeginSeq;
        for (const auto& id : m_Atlas->GetData().Sprites)
            out << id;
        out << YAML::EndSeq << YAML::EndMap;
        return out.c_str();
    }

    bool SpriteAtlasSerializer::Serialize(const Path& path) const
    {
        return m_Atlas && !path.empty() && FileSystem::WriteTextFileAtomic(path, SerializeToString());
    }

    bool SpriteAtlasSerializer::Deserialize(const Path& path)
    {
        if (!DeserializeFromString(FileSystem::ReadTextFile(path)))
            return false;
        if (m_Atlas->GetName().empty())
            m_Atlas->SetName(path.stem().string());
        return true;
    }

    bool SpriteAtlasSerializer::DeserializeFromString(const String& text)
    {
        if (!m_Atlas)
            return false;
        try
        {
            const auto source = YAML::Load(text);
            if (!source.IsMap() || source["Version"].as<uint32_t>(0) != SPRITE_ATLAS_FORMAT_VERSION)
                return false;
            SpriteAtlasData data;
            String name;
            if (source["Name"])
                name = source["Name"].as<String>();
            if (source["PageSize"])
                data.PageSize = source["PageSize"].as<uint32_t>();
            if (source["MipLevels"])
                data.MipLevels = source["MipLevels"].as<uint32_t>();
            if (const auto sprites = source["Sprites"])
            {
                if (!sprites.IsSequence() || sprites.size() > 65536)
                    return false;
                for (const auto& id : sprites)
                    data.Sprites.push_back(id.as<UUID>());
            }
            if (!m_Atlas->SetData(data))
                return false;
            m_Atlas->SetName(name);
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
} // namespace Crowny
