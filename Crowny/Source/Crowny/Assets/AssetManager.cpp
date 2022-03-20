#include "cwpch.h"

#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"

#include "Crowny/Audio/AudioSource.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"

// TODO: Fix Audio clips being loaded every time play in inspector is used.

namespace Crowny
{

    void Save(BinaryDataStreamOutputArchive& archive, const Asset& asset) { archive(asset.m_KeepData, asset.m_Name); }

    void Load(BinaryDataStreamInputArchive& archive, Asset& asset) { archive(asset.m_KeepData, asset.m_Name); }

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
                                            pixelData->GetSize())); // TODO: Save more pixel data (wat does this mean, maybe pixel data serializer?)?
            }
        }
    }

    void Save(BinaryDataStreamOutputArchive& archive, const ScriptCode& code)
	{
		archive(cereal::base_class<Asset>(&code));
        archive(code.m_Source);
    }

	void Load(BinaryDataStreamInputArchive& archive, ScriptCode& code)
	{
		archive(cereal::base_class<Asset>(&code));
		archive(code.m_Source);
	}


	template <typename Archive> void Serialize(Archive& archive, TextureImportOptions& importOptions)
	{
		archive(importOptions.AutomaticFormat, importOptions.CpuCached, importOptions.Format,
			importOptions.GenerateMips, importOptions.MaxMip, importOptions.Shape, importOptions.SRGB);
	}

	template <typename Archive> void Serialize(Archive& archive, AudioClipImportOptions& importOptions)
	{
		archive(importOptions.Format, importOptions.Quality, importOptions.ReadMode, importOptions.BitDepth,
			importOptions.Is3D);
	}

	void Save(BinaryDataStreamOutputArchive& archive, const ShaderImportOptions& importOptions)
	{
		archive(importOptions.Language, importOptions.m_Defines);
	}

	void Load(BinaryDataStreamInputArchive& archive, ShaderImportOptions& importOptions)
	{
		archive(importOptions.Language, importOptions.m_Defines);
	}

	template <typename Archive> void Serialize(Archive& archive, CSharpScriptImportOptions& importOptions)
	{
		archive(importOptions.IsEditorScript);
	}

    template <class Archive> void Serialize(Archive& archive, UniformDesc& desc)
    {
        archive(desc.Uniforms, desc.Samplers, desc.Textures, desc.LoadStoreTextures);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const Shader& shader)
    {
        // TODO: Fix these for all stages
        archive(shader.m_ShaderStages[VERTEX_SHADER]->m_ShaderData->Data);
        archive(shader.m_ShaderStages[VERTEX_SHADER]->m_ShaderData->EntryPoint);
        archive(shader.m_ShaderStages[VERTEX_SHADER]->m_ShaderData->Type);
        archive(shader.m_ShaderStages[VERTEX_SHADER]->m_ShaderData->Description);

		archive(shader.m_ShaderStages[FRAGMENT_SHADER]->m_ShaderData->Data);
		archive(shader.m_ShaderStages[FRAGMENT_SHADER]->m_ShaderData->EntryPoint);
		archive(shader.m_ShaderStages[FRAGMENT_SHADER]->m_ShaderData->Type);
		archive(shader.m_ShaderStages[FRAGMENT_SHADER]->m_ShaderData->Description);
        // archive(shader.m_ShaderStages[FRAGMENT_SHADER]);
        // archive(shader.m_ShaderStages);
        //for (uint32_t i = 0; i < SHADER_COUNT; i++)
        //{
        //    // if (shader.m_ShaderStages[i])
        //        archive(shader.m_ShaderStages[i]);
        //}
    }

    void Load(BinaryDataStreamInputArchive& archive, Shader& shader)
    {
        Ref<BinaryShaderData> data = CreateRef<BinaryShaderData>();
		archive(data->Data);
		archive(data->EntryPoint);
		archive(data->Type);
		archive(data->Description);
        shader.m_ShaderStages[VERTEX_SHADER] = ShaderStage::Create(data);

		data = CreateRef<BinaryShaderData>();
		archive(data->Data);
		archive(data->EntryPoint);
		archive(data->Type);
		archive(data->Description);
		shader.m_ShaderStages[FRAGMENT_SHADER] = ShaderStage::Create(data);
        // archive(shader.m_ShaderStages);
        /*for (uint32_t i = 0; i < SHADER_COUNT; i++)
            archive(shader.m_ShaderStages[i]);*/
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
        /*
        auto findIter = m_LoadedAssets.find(uuid);
        if (findIter != m_LoadedAssets.end())
            return findIter->second;
        AssetHandle assetHandle;
        auto findIter = m_Handles.find(uuid);
        if (findIter != m_Handles.end())
            assetHandle = findIter->second;
        else
        {
            assetHandle = AssetHandle(uuid);
            m_Handles[uuid] = assetHandle.GetWeakHandle();
        }

        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        BinaryDataStreamInputArchive archive(stream);
        Ref<Asset> asset;
        archive(asset);
        assetHandle.SetAssetHandle(asset);
        m_LoadedAssets[uuid] = asset;
        return assetHandle;*/

        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        BinaryDataStreamInputArchive archive(stream);
        Ref<Asset> asset;
        archive(asset);
        return asset;
    }

    void AssetManager::Save(const Ref<Asset>& resource, const Path& filepath)
    {
        if (!fs::is_directory(filepath.parent_path()))
            fs::create_directories(filepath.parent_path());
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(filepath);
        BinaryDataStreamOutputArchive archive(stream);
        archive(resource);
        stream->Close();
    }

    void AssetManager::RegisterAssetManifest(const Ref<AssetManifest>& manifest)
    {
        auto iterFind = std::find(m_Manifests.begin(), m_Manifests.end(), manifest);
        if (iterFind == m_Manifests.end())
            m_Manifests.push_back(manifest);
        else
            *iterFind = manifest;
    }

    void AssetManager::UnregisterAssetManifest(const Ref<AssetManifest>& manifest)
    {
        auto iterFind = std::find(m_Manifests.begin(), m_Manifests.end(), manifest);
        if (iterFind != m_Manifests.end())
            m_Manifests.erase(iterFind);
    }

    void AssetManager::GetFilepathFromUUID(const UUID& uuid, Path& outFilepath)
    {
        for (auto& manifest : m_Manifests)
        {
            if (manifest->UuidToFilepath(uuid, outFilepath))
                return;
        }
    }
    bool AssetManager::GetUUIDFromFilepath(const Path& filepath, UUID& outUUID)
    {
        // broken
        for (auto& manifest : m_Manifests)
        {
            if (manifest->FilepathToUuid(filepath, outUUID))
                return true;
        }
        return false;
    }

} // namespace Crowny

CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::AudioClip, "AudioClip")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::Texture, "Texture")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::ScriptCode, "ScriptCode")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::Shader, "Shader")
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Shader)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::AudioClip)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Texture)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::ScriptCode)
CEREAL_REGISTER_DYNAMIC_INIT(AssetManager)