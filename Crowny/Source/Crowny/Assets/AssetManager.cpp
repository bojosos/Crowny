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
CEREAL_REGISTER_TYPE(Crowny::Texture)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::AudioClip)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Texture)

namespace Crowny
{

    void Save(BinaryDataStreamOutputArchive& archive, const glm::vec2& vec) { archive(vec.x, vec.y); }
    void Load(BinaryDataStreamInputArchive& archive, glm::vec2& vec) { archive(vec.x, vec.y); }

    void Save(BinaryDataStreamOutputArchive& archive, const glm::vec3& vec) { archive(vec.x, vec.y, vec.z); }
    void Load(BinaryDataStreamInputArchive& archive, glm::vec3& vec) { archive(vec.x, vec.y, vec.z); }

    void Save(BinaryDataStreamOutputArchive& archive, const glm::vec4& vec) { archive(vec.x, vec.y, vec.z, vec.w); }
    void Load(BinaryDataStreamInputArchive& archive, glm::vec4& vec) { archive(vec.x, vec.y, vec.z, vec.w); }

    void Save(BinaryDataStreamOutputArchive& archive, const UUID& uuid)
    {
        archive(uuid.m_Data[0], uuid.m_Data[1], uuid.m_Data[2], uuid.m_Data[3]);
    }
    void Load(BinaryDataStreamInputArchive& archive, UUID& uuid)
    {
        archive(uuid.m_Data[0], uuid.m_Data[1], uuid.m_Data[2], uuid.m_Data[3]);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const Asset& asset) { archive(asset.m_KeepData); }

    void Load(BinaryDataStreamInputArchive& archive, Asset& asset) { archive(asset.m_KeepData); }

    void Load(BinaryDataStreamInputArchive& archive, AudioClip& clip)
    {
        archive(cereal::base_class<Asset>(&clip)); // Save asset base class
        AudioClipDesc& desc = clip.m_Desc;         // Save clip desc
        archive(desc.ReadMode, desc.Format, desc.Frequency, desc.BitDepth, desc.NumChannels, desc.Is3D);
        archive(clip.m_Length, clip.m_NumSamples);
        CW_ENGINE_INFO("Loaded: {0}", clip.m_NumSamples);

        archive(clip.m_StreamSize); // Load the size of audio data
        clip.m_StreamData = archive.GetStream()->Clone();
        clip.m_StreamOffset = (uint32_t)archive.GetStream()->Tell();
        clip.Init();
    }

    void Save(BinaryDataStreamOutputArchive& archive, const AudioClip& clip)
    {
        archive(cereal::base_class<Asset>(&clip)); // Save asset base class
        const AudioClipDesc& desc = clip.m_Desc;   // Save clip desc
        archive(desc.ReadMode, desc.Format, desc.Frequency, desc.BitDepth, desc.NumChannels, desc.Is3D);
        archive(clip.m_Length, clip.m_NumSamples);

        uint32_t size = 0; // Save the samples
        auto sourceStream = clip.GetSourceStream(size);
        Vector<uint8_t> samples(size);
        sourceStream->Read(samples.data(), size); // Save the stream data
        archive(size);
        archive(cereal::binary_data(samples.data(), size));
    }

    void Load(BinaryDataStreamInputArchive& archive, Texture& texture)
    {
        archive(cereal::base_class<Asset>(&texture));
        TextureParameters& params = texture.m_Params;
        archive(params.Width, params.Height, params.Depth, params.MipLevels, params.Samples, params.Type, params.Shape,
                params.Format);
        for (uint32_t mip = 0; mip < params.MipLevels; mip++)
        {
            for (uint32_t face = 0; face < params.Faces; face++)
            {
                uint32_t size = 0;
                archive(size);
                Ref<PixelData> pixelData = CreateRef<PixelData>(texture.GetWidth(), texture.GetHeight(),
                                                                texture.GetDepth(), texture.GetFormat());
                archive.GetStream()->Read(pixelData->GetData(), size);
                texture.WriteData(*pixelData, mip, face);
            }
        }
    }

    void Save(BinaryDataStreamOutputArchive& archive, Texture& texture)
    {
        archive(cereal::base_class<Asset>(&texture));
        const TextureParameters& params = texture.GetProperties();
        archive(params.Width, params.Height, params.Depth, params.MipLevels, params.Samples, params.Type, params.Shape,
                params.Format);
        texture.Init();
        for (uint32_t mip = 0; mip < params.MipLevels + 1; mip++) // Save all texture data
        {
            for (uint32_t face = 0; face < params.Faces; face++)
            {
                Ref<PixelData> pixelData = texture.AllocatePixelData(face, mip);
                texture.ReadData(*pixelData, face, mip);
                archive(cereal::binary_data((uint8_t*)pixelData->GetData(),
                                            pixelData->GetSize())); // TODO: Save more pixel data
            }
        }
    }

    Ref<Asset> AssetManager::Load(const Path& filepath, bool keepSourceData)
    {
        if (!FileSystem::FileExists(filepath))
        {
            CW_ENGINE_WARN("Resource {0} does not exist.", filepath);
            return nullptr;
        }

        UUID uuid;
        bool exists = GetUUIDFromFilepath(filepath, uuid);
        if (!exists)
            uuid = UuidGenerator::Generate();
        return Load(uuid, filepath, keepSourceData);
    }

    Ref<Asset> AssetManager::LoadFromUUID(const UUID& uuid, bool keepSourceData)
    {
        Path filepath;
        GetFilepathFromUUID(uuid, filepath);
        return Load(uuid, filepath, keepSourceData);
    }

    Ref<Asset> AssetManager::Load(const UUID& uuid, const Path& filepath, bool keepSourceData)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        BinaryDataStreamInputArchive archive(stream);
        Ref<Asset> output;
        archive(output);
        return output;
    }

    void AssetManager::Save(const Ref<Asset>& resource, const Path& filepath)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(filepath, false);
        BinaryDataStreamOutputArchive archive(stream);
        archive(resource);
    }

    void AssetManager::GetFilepathFromUUID(const UUID& uuid, Path& outFilepath) {}
    bool AssetManager::GetUUIDFromFilepath(const Path& filepath, UUID& outUUID) { return true; }

} // namespace Crowny
