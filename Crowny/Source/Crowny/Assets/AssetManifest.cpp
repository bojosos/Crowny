#include "cwpch.h"

#include "Crowny/Assets/AssetManifest.h"

#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/Common/Yaml.h"

namespace Crowny
{

    AssetManifest::AssetManifest(const String& name) : m_Name(name) {}

    /*

    Source = gAudio().CreateSource();
    Ref<DataStream> stream = FileSystem::OpenFile("test.ogg", true);
    Ref<OggVorbisDecoder> decoder = CreateRef<OggVorbisDecoder>();
    AudioDataInfo info;
    decoder->Open(stream, info);
    uint32_t bps = info.BitDepth / 8;
    uint32_t bufferSize = info.NumSamples * bps;
    Ref<MemoryDataStream> samples = CreateRef<MemoryDataStream>(bufferSize);
    decoder->Read(samples->Data(), info.NumSamples);

    { // covert to mono
        if (info.NumChannels > 1)
        {
            uint32_t samplesPerChannel = info.NumSamples / info.NumChannels;
            uint32_t monoBufferSize = samplesPerChannel * bps;
            CW_ENGINE_INFO("{0}, {1}", info.NumSamples, info.NumChannels);
            Ref<MemoryDataStream> monoStream = CreateRef<MemoryDataStream>(monoBufferSize);
            CW_ENGINE_INFO(bps);
            AudioUtils::ConvertToMono(samples->Data(), monoStream->Data(), info.BitDepth, samplesPerChannel,
                                        info.NumChannels);
            info.NumSamples = samplesPerChannel;
            info.NumChannels = 1;
            samples = monoStream;
            bufferSize = monoBufferSize;
        }
    }
    AudioClipDesc clipDesc;
    clipDesc.BitDepth = info.BitDepth;
    clipDesc.Format = AudioFormat::VORBIS;
    clipDesc.Frequency = info.SampleRate;
    clipDesc.NumChannels = info.NumChannels;
    clipDesc.ReadMode = AudioReadMode::LoadDecompressed;
    clipDesc.Is3D = true;
    Ref<AudioClip> clip = CreateRef<AudioClip>(stream, bufferSize, info.NumSamples, clipDesc);
    Source->SetClip(clip);
    */
    /*
        void AssetManifest::Serialize(const Path& filepath)
        {
            YAML::Emitter out;
            out << YAML::Comment("Crowny Manifest");

            out << YAML::BeginMap;
            out << YAML::Key << "Manifest" << YAML::Value << m_Name;
            out << YAML::Key << "Resources" << YAML::Value;

            out << YAML::BeginSeq;
            for (auto& it : m_Paths)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "UUID" << YAML::Value << it.first;
                out << YAML::Key << "Path" << YAML::Value << it.second;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq << YAML::EndMap;

            VirtualFileSystem::Get()->WriteTextFile(filepath, out.c_str());
        }
    *//*
    void AssetManifest::Deserialize(const Path& filepath)
    {
        String text = VirtualFileSystem::Get()->ReadTextFile(filepath);
        YAML::Node data = YAML::Load(text);

        if (!data["Manifest"])
            return;

        m_Name = data["Manifest"].as<String>();
        auto rss = data["Resources"];
        for (auto resource : rss)
        {
            UUID id = resource["UUID"].as<UUID>();
            String path = resource["Path"].as<String>();
            m_UUIDs[path] = id;
            m_Paths[id] = path;
        }
    }
*/
} // namespace Crowny