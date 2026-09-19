#include "cwpch.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/Yaml.h"
#include "Crowny/Serialization/SpriteAnimationSerializer.h"

namespace Crowny
{
    String SpriteAnimationSerializer::SerializeToString() const
    {
        if (!m_Clip)
            return {};
        YAML::Emitter out;
        out << YAML::BeginMap;
        SerializeValueYAML(out, "Version", SPRITE_ANIMATION_CLIP_FORMAT_VERSION);
        SerializeValueYAML(out, "Name", m_Clip->GetName());
        SerializeEnumYAML(out, "Mode", m_Clip->GetData().Mode);
        out << YAML::Key << "Frames" << YAML::Value << YAML::BeginSeq;
        for (const auto& frame : m_Clip->GetData().Frames)
        {
            out << YAML::BeginMap;
            SerializeValueYAML(out, "Sprite", frame.SpriteId);
            SerializeValueYAML(out, "Duration", frame.Duration);
            out << YAML::EndMap;
        }
        out << YAML::EndSeq << YAML::EndMap;
        return out.c_str();
    }

    bool SpriteAnimationSerializer::Serialize(const Path& path) const
    {
        return m_Clip && !path.empty() && FileSystem::WriteTextFileAtomic(path, SerializeToString());
    }

    bool SpriteAnimationSerializer::Deserialize(const Path& path)
    {
        if (!DeserializeFromString(FileSystem::ReadTextFile(path)))
            return false;
        if (m_Clip->GetName().empty())
            m_Clip->SetName(path.stem().string());
        return true;
    }

    bool SpriteAnimationSerializer::DeserializeFromString(const String& text)
    {
        if (!m_Clip)
            return false;
        try
        {
            const auto source = YAML::Load(text);
            if (!source.IsMap() || source["Version"].as<uint32_t>(0) != SPRITE_ANIMATION_CLIP_FORMAT_VERSION)
                return false;
            SpriteAnimationClipData data;
            String name;
            if (source["Name"])
                name = source["Name"].as<String>();
            if (source["Mode"])
            {
                const auto mode = source["Mode"].as<uint32_t>();
                if (mode > static_cast<uint32_t>(SpriteAnimationMode::PingPong))
                    return false;
                data.Mode = static_cast<SpriteAnimationMode>(mode);
            }
            if (const auto frames = source["Frames"])
            {
                if (!frames.IsSequence() || frames.size() > 65536)
                    return false;
                for (const auto& frame : frames)
                    data.Frames.push_back({ frame["Sprite"].as<UUID>(), frame["Duration"].as<float>() });
            }
            if (!m_Clip->SetData(data))
                return false;
            m_Clip->SetName(name);
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
} // namespace Crowny
