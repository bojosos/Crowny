#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"

#include "Crowny/Assets/CerealDataStreamArchive.h"
#include "Crowny/Audio/AudioSource.h"

#include <cereal/cereal.hpp>

#include <cereal/archives/portable_binary.hpp>

#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/vector.hpp>

#include <fstream>

CEREAL_REGISTER_TYPE(Crowny::AudioClip)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::AudioClip)

namespace Crowny
{

    template <class Archive> void save(Archive& archive, const Asset& asset) { archive(asset.m_KeepData); }

    template <typename Archive> void load(Archive& archive, Asset& asset) { archive(asset.m_KeepData); }

    template <typename Archive> void load(Archive& archive, AudioClip& clip)
    {
        archive(cereal::base_class<Asset>(&clip)); // Save asset base class
        AudioClipDesc& desc = clip.m_Desc;         // Save clip desc
        archive(desc.ReadMode, desc.Format, desc.Frequency, desc.BitDepth, desc.NumChannels, desc.Is3D);
        CW_ENGINE_INFO("Readmode {0}, format: {1}, freq: {2}, bitdepth: {3}, numchannels: {4}, is3d: {5}",
                       desc.ReadMode, desc.Format, desc.Frequency, desc.BitDepth, desc.NumChannels, desc.Is3D);
        archive(clip.m_Length, clip.m_NumSamples);
        CW_ENGINE_INFO("Length: {0}, samples: {1}", clip.m_Length, clip.m_NumSamples);
        // std::vector<uint8_t> samples(clip.GetNumSamples()); // Save the samples
        // clip.SetSamples(samples.data(), 0 , samples.size());
        // archive(samples);
    }

    template <class Archive> void save(Archive& archive, const AudioClip& clip)
    {
        archive(cereal::base_class<Asset>(&clip)); // Save asset base class
        const AudioClipDesc& desc = clip.m_Desc;   // Save clip desc
        archive(desc.ReadMode, desc.Format, desc.Frequency, desc.BitDepth, desc.NumChannels, desc.Is3D);
        archive(clip.m_Length, clip.m_NumSamples);
        CW_ENGINE_INFO("Readmode {0}, format: {1}, freq: {2}, bitdepth: {3}, numchannels: {4}, is3d: {5}",
                       desc.ReadMode, desc.Format, desc.Frequency, desc.BitDepth, desc.NumChannels, desc.Is3D);
        CW_ENGINE_INFO("Length: {0}, samples: {1}", clip.m_Length, clip.m_NumSamples);

        uint32_t size = 0; // Save the samples
        auto sourceStream = clip.GetSourceStream(size);
        std::vector<uint8_t> samples(clip.GetNumSamples());
        sourceStream->Read(samples.data(), size);
        archive(samples);
    }

    Ref<Asset> AssetManager::Load(const std::string& filepath, bool keepSourceData)
    {
        //        if (!FileSystem::FileExists(filepath))
        {
            //        CW_ENGINE_WARN("Resource {0} does not exist.", filepath);
            //          return nullptr;
        }

        Uuid uuid;
        bool exists = GetUuidFromFilepath(filepath, uuid);
        if (!exists)
            uuid = UuidGenerator::Generate();
        return Load(uuid, filepath, keepSourceData);
    }

    Ref<Asset> AssetManager::LoadFromUUID(const Uuid& uuid, bool keepSourceData)
    {
        std::string filepath;
        GetFilepathFromUuid(uuid, filepath);
        return Load(uuid, filepath, keepSourceData);
    }

    Ref<Asset> AssetManager::Load(const Uuid& uuid, const std::string& filepath, bool keepSourceData)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        BinaryDataStreamInputArchive archive(stream);
        Ref<Asset> output;
        archive(output);
        return output;
    }

    void AssetManager::Save(const Ref<Asset>& resource, const std::string& filepath)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath, false);
        BinaryDataStreamOutputArchive archive(stream);
        archive(resource);
    }

    void AssetManager::GetFilepathFromUuid(const Uuid& uuid, std::string& outFilepath) {}
    bool AssetManager::GetUuidFromFilepath(const std::string& filepath, Uuid& outUuid) { return true; }

} // namespace Crowny
