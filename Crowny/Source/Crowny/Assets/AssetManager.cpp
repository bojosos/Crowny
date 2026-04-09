#include "cwpch.h"

#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"

#include "Crowny/Audio/AudioSource.h"
#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/MSDFdata.h"
#include "Crowny/Renderer/Mesh.h"

#include "Platform/Vulkan/VulkanTexture.h"

#include "Crowny/Utils/Compression.h"

#include <cereal/types/utility.hpp>

// These have to be outside of our namespace....
template <typename Archive> void Serialize(Archive& archive, msdf_atlas::GlyphGeometry& glyph)
{
    archive(glyph.advance, glyph.geometryScale, glyph.box.rect.x, glyph.box.rect.y, glyph.box.rect.w, glyph.box.rect.h, glyph.box.range,
            glyph.box.scale, glyph.box.translate.x, glyph.box.translate.y);
}

template <typename Archive> void Serialize(Archive& archive, msdfgen::FontMetrics& fontMetrics)
{
    archive(fontMetrics.ascenderY, fontMetrics.descenderY, fontMetrics.lineHeight);
}

void Save(BinaryDataStreamOutputArchive& archive, const msdf_atlas::FontGeometry& fontGeometry)
{
    archive(fontGeometry.glyphsByIndex);
    archive(*fontGeometry.glyphs);
    archive(fontGeometry.kerning);
    archive(fontGeometry.glyphsByCodepoint);
    archive(fontGeometry.metrics);
}

void Load(BinaryDataStreamInputArchive& archive, msdf_atlas::FontGeometry& fontGeometry)
{
    archive(fontGeometry.glyphsByIndex);
    archive(fontGeometry.ownGlyphs);
    archive(fontGeometry.kerning);
    archive(fontGeometry.glyphsByCodepoint);
    archive(fontGeometry.metrics);
    fontGeometry.glyphs = &fontGeometry.ownGlyphs;
}

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
        // CW_ENGINE_INFO("Loaded: {0}", clip.m_NumSamples);

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

    void Save(BinaryDataStreamOutputArchive& archive, const Font& font)
    {
        archive(cereal::base_class<Asset>(&font));
        const MSDFData* const data = font.GetMSDFData();
        archive(data->FontGeometry);
        archive(data->Glyphs);
        archive(font.m_AtlasTexture);
    }

    void Load(BinaryDataStreamInputArchive& archive, Font& font)
    {
        archive(cereal::base_class<Asset>(&font));
        font.m_MSDFData = new MSDFData();
        archive(font.m_MSDFData->FontGeometry);
        archive(font.m_MSDFData->Glyphs);
        archive(font.m_AtlasTexture);
    }

    void Load(BinaryDataStreamInputArchive& archive, Texture& texture)
    {
        archive(cereal::base_class<Asset>(&texture));
        TextureParameters& params = texture.m_Params;
        archive(params.Type, params.Shape, params.sRGB, params.ReadWrite, params.GenerateMipmaps, params.MipLevels, params.Samples, params.Faces,
                params.Width, params.Height, params.Depth, params.Usage, params.Format);
        texture.Init();

        for (uint32_t mip = 0; mip < params.MipLevels + 1; mip++)
        {
            for (uint32_t face = 0; face < params.Faces; face++)
            {
                Ref<PixelData> pixelData = CreateRef<PixelData>(texture.GetWidth(), texture.GetHeight(), texture.GetDepth(), texture.GetFormat());
                pixelData->AllocateInternalBuffer();
                archive(cereal::binary_data((uint8_t*)pixelData->GetData(), pixelData->GetSize()));
                texture.WriteData(*pixelData, mip, face);
            }
        }
    }

    void Save(BinaryDataStreamOutputArchive& archive, const Texture& texture)
    {
        Texture& texture2 = const_cast<Texture&>(texture);

        archive(cereal::base_class<Asset>(&texture2));
        const TextureParameters& params = texture2.GetProperties();
        archive(params.Type, params.Shape, params.sRGB, params.ReadWrite, params.GenerateMipmaps, params.MipLevels, params.Samples, params.Faces,
                params.Width, params.Height, params.Depth, params.Usage, params.Format);
        for (uint32_t mip = 0; mip < params.MipLevels + 1; mip++) // Save all texture data
        {
            for (uint32_t face = 0; face < params.Faces; face++)
            {
                Ref<PixelData> pixelData = texture2.AllocatePixelData(face, mip);
                texture2.ReadData(*pixelData, face, mip);
                archive(cereal::binary_data((uint8_t*)pixelData->GetData(),
                                            pixelData->GetSize())); // TODO: Save more pixel data (wat does this mean,
                                                                    // maybe pixel data serializer?)?
            }
        }
    }

    void Save(BinaryDataStreamOutputArchive& archive, const VulkanTexture& texture) { archive(cereal::base_class<Texture>(&texture)); }

    void Load(BinaryDataStreamInputArchive& archive, VulkanTexture& texture) { archive(cereal::base_class<Texture>(&texture)); }

    void Load(BinaryDataStreamInputArchive& archive, Mesh& mesh)
    {
        archive(cereal::base_class<Asset>(&mesh));
        archive(mesh.m_Layout);
        archive(mesh.m_IndexType);
        archive(mesh.m_DrawMode);
        archive(mesh.m_NumIndices);
        archive(mesh.m_NumVertices);
        archive(mesh.m_DrawMode);
        uint32_t usage = 0;
        archive(usage);
        mesh.m_Usage = MeshUsageFlags(usage);
        archive(mesh.m_CPUMeshData);
        mesh.Init();
        if (!mesh.m_Usage.IsSet(MeshUsage::CpuCached))
            mesh.m_CPUMeshData = nullptr;
    }

    void Save(BinaryDataStreamOutputArchive& archive, const Mesh& constMesh)
    {
        Mesh& mesh = const_cast<Mesh&>(constMesh);
        archive(cereal::base_class<Asset>(&mesh));
        archive(mesh.m_Layout);
        archive(mesh.m_IndexType);
        archive(mesh.m_DrawMode);
        archive(mesh.m_NumIndices);
        archive(mesh.m_NumVertices);
        archive(mesh.m_DrawMode);
        archive((uint32_t)mesh.m_Usage);
        Ref<MeshData> meshData = mesh.AllocBuffer();
        mesh.ReadData(meshData);
        archive(meshData);
    }

    void Load(BinaryDataStreamInputArchive& archive, MeshData& meshData)
    {
        archive(meshData.m_IndexType);
        archive(meshData.m_NumVertices);
        archive(meshData.m_NumIndices);
        archive(meshData.m_Layout);
        meshData.AllocateBuffer();
        archive(cereal::binary_data(meshData.m_Data, meshData.GetIndexBufferSize() + meshData.GetVertexBufferSize()));
    }

    void Save(BinaryDataStreamOutputArchive& archive, const MeshData& meshData)
    {
        archive(meshData.m_IndexType);
        archive(meshData.m_NumVertices);
        archive(meshData.m_NumIndices);
        archive(meshData.m_Layout);
        archive(cereal::binary_data(meshData.m_Data, meshData.GetIndexBufferSize() + meshData.GetVertexBufferSize()));
    }

    template <typename Archive> void Serialize(Archive& archive, BufferElement& element)
    {
        archive(element.Attribute);
        archive(element.Name);
        archive(element.Normalized);
        archive(element.Offset);
        archive(element.Size);
        archive(element.Type);
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
        archive(importOptions.AutomaticFormat, importOptions.CpuCached, importOptions.Format, importOptions.GenerateMips, importOptions.MaxMip,
                importOptions.Shape, importOptions.SRGB, importOptions.DiskFormat);
    }

    template <typename Archive> void Serialize(Archive& archive, AudioClipImportOptions& importOptions)
    {
        archive(importOptions.Format, importOptions.Quality, importOptions.ReadMode, importOptions.BitDepth, importOptions.Is3D);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const ShaderImportOptions& importOptions)
    {
        archive(importOptions.Language, importOptions.m_Defines);
    }

    void Load(BinaryDataStreamInputArchive& archive, ShaderImportOptions& importOptions) { archive(importOptions.Language, importOptions.m_Defines); }

    template <typename Archive> void Serialize(Archive& archive, CSharpScriptImportOptions& importOptions) { archive(importOptions.IsEditorScript); }

    template <class Archive> void Serialize(Archive& archive, UniformDesc& desc)
    {
        archive(desc.Uniforms, desc.Samplers, desc.Textures, desc.LoadStoreTextures);
    }

    void Load(BinaryDataStreamInputArchive& archive, PhysicsMaterial2D& material)
    {
        archive(material.m_Density, material.m_Friction, material.m_Restitution, material.m_RestitutionThreshold);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const PhysicsMaterial2D& material)
    {
        archive(material.m_Density, material.m_Friction, material.m_Restitution, material.m_RestitutionThreshold);
    }

    void Load(BinaryDataStreamInputArchive& archive, BufferLayout& layout)
    {
        archive(layout.m_Id, layout.m_Elements);
        layout.CalculateOffsetsAndStride();
        BufferLayout::s_NextFreeId = std::max(BufferLayout::s_NextFreeId, layout.m_Id + 1);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const BufferLayout& layout)
    {
        archive(layout.m_Id, layout.m_Elements);
    }

    template <typename Archive> void Serialize(Archive& archive, BinaryShaderData& binaryShaderData)
    {
        archive(binaryShaderData.Data, binaryShaderData.EntryPoint, binaryShaderData.Type, binaryShaderData.Description,
                binaryShaderData.VertexLayout);
    }

    template <typename Archive> void Serialize(Archive& archive, BlendStateDesc& stateDesc)
    {
        archive(cereal::binary_data(&stateDesc, sizeof(BlendStateDesc)));
    }
    
    template <typename Archive> void Serialize(Archive& archive, RasterizerStateDesc& stateDesc)
    {
        archive(cereal::binary_data(&stateDesc, sizeof(RasterizerStateDesc)));
    }

    template <typename Archive> void Serialize(Archive& archive, DepthStencilStateDesc& stateDesc)
    {
        archive(cereal::binary_data(&stateDesc, sizeof(DepthStencilStateDesc)));
    }

    template <typename Archive> void Serialize(Archive& archive, ShaderRenderPassDesc& renderPass)
    {
        archive(renderPass.BlendState);
        archive(renderPass.RasterizationState);
        archive(renderPass.DepthStencilState);
        archive(renderPass.StencilValue);
        archive(renderPass.VertexShader);
        archive(renderPass.FragmentShader);
        archive(renderPass.GeometryShader);
        archive(renderPass.HullShader);
        archive(renderPass.DomainShader);
        archive(renderPass.ComputeShader);
        archive(renderPass.RaygenShader);
        archive(renderPass.MissShader);
        archive(renderPass.HitShader);
    }

    template <typename Archive> void Serialize(Archive& archive, ShaderVariation::Specifier& specifier)
    {
        archive(specifier.Name, specifier.Type, specifier.I);
    }

    template <typename Archive> void Serialize(Archive& archive, ShaderVariation& shaderVariation) { archive(shaderVariation.m_Parameters); }
    
    template <typename Archive> void Serialize(Archive& archive, ShaderRenderPass& renderPass) { archive(renderPass.m_ShaderDesc); }

    template <typename Archive> void Serialize(Archive& archive, ShaderTechnique& shaderTechnique)
    {
        archive(shaderTechnique.m_Passes, shaderTechnique.m_Tags, shaderTechnique.m_Variation);
    }

    void Load(BinaryDataStreamInputArchive& archive, Shader& shader)
    {
        archive(cereal::base_class<Asset>(&shader));
        archive(shader.m_Techniques);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const Shader& shader)
    {
        archive(cereal::base_class<Asset>(&shader));
        archive(shader.m_Techniques);
    }

    AssetHandle<Asset> AssetManager::Load(const Path& filepath, bool keepInternalRef, bool keepSourceData)
    {
        if (!fs::exists(filepath))
        {
            CW_ENGINE_WARN("Resource {0} does not exist.", filepath);
            return nullptr;
        }

        UUID uuid;
        bool exists = GetUUIDFromFilepath(filepath, uuid);
        if (!exists)
            uuid = UuidGenerator::Generate();
        return Load(uuid, filepath, keepInternalRef, keepSourceData);
    }

    AssetHandle<Asset> AssetManager::LoadFromUUID(const UUID& uuid, bool keepInternalRef, bool keepSourceData)
    {
        auto iterFind = m_Handles.find(uuid);
        if (iterFind != m_Handles.end())
            return iterFind->second.Lock();
        Path filepath;
        GetFilepathFromUUID(uuid, filepath);
        if (filepath.empty() || !fs::exists(filepath))
        {
            return AssetHandle<Asset>();
        }
        return Load(uuid, filepath, keepInternalRef, keepSourceData);
    }

    AssetHandle<Asset> AssetManager::Load(const UUID& uuid, const Path& filepath, bool keepInternalRef, bool keepSourceData)
    {
        auto iterFind = m_Handles.find(uuid);
        if (iterFind != m_Handles.end())
            return iterFind->second.Lock();

        Ref<DataStream> stream = FileSystem::OpenFile(filepath);
        BinaryDataStreamInputArchive archive(stream);
        Ref<Asset> asset;
        archive(asset);
        AssetHandle<Asset> output = AssetHandle<Asset>(asset, uuid);
        m_Handles[uuid] = output.GetWeak();
        return output;
    }

    AssetHandle<Asset> AssetManager::GetAssetHandle(const UUID& uuid)
    {
        auto iterFind = m_Handles.find(uuid);
        if (iterFind != m_Handles.end())
            return iterFind->second.Lock();
        AssetHandle<Asset> handle(uuid);
        m_Handles[uuid] = handle.GetWeak();
        return handle;
    }

    void AssetManager::Save(const Ref<Asset>& resource, const Path& filepath)
    {
        if (!fs::is_directory(filepath.parent_path()))
            fs::create_directories(filepath.parent_path());
        // Ref<MemoryDataStream> memStream = CreateRef<MemoryDataStream>();
        //      BinaryDataStreamOutputArchive archive(memStream);
        //      archive(resource);
        //// TODO: Check if a file is worth compressing, if not, just write the data to the file
        //// No need to compress already compressed files (images, audio, ...).
        //
        // Vector<uint8_t> result;
        //// This buffer might be too small.
        //      result.resize(memStream->Size());; // Maybe if I do this in chunks I can avoid this big alloc, since
        //      most of this allocate data won't be used.
        // Compression::Compress(result.data(), memStream->Data(), memStream->Size(), CompressionMethod::FastLZ);
        //      Ref<DataStream> stream = FileSystem::CreateAndOpenFile(filepath);
        // stream->Write(result.data(), result.size());
        //      stream->Close();
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

    AssetHandle<Asset> AssetManager::CreateAssetHandle(const Ref<Asset>& asset)
    {
        UUID uuid = UuidGenerator::Generate();
        return CreateAssetHandle(asset, uuid);
    }

    AssetHandle<Asset> AssetManager::CreateAssetHandle(const Ref<Asset>& asset, const UUID& uuid)
    {
        AssetHandle<Asset> newHandle(asset, uuid);

        if (asset)
        {
            // LoadedResourceData& resData = m_LoadedResources[uuid];
            // resData.resource = newHandle.GetWeak();
        }

        m_Handles[uuid] = newHandle.GetWeak();
        return newHandle;
    }

    void AssetManager::Release(AssetHandleBase& handle)
    {
        auto iterFind = m_LoadedAssets.find(handle.GetUUID());
        if (iterFind != m_LoadedAssets.end())
        {
            // LoadedResourceData& resData = iterFind->second;

            // assert(resData.numInternalRefs > 0);
            // resData.numInternalRefs--;
            // resource.removeInternalRef();

            // std::uint32_t refCount = resource.getHandleData()->mRefCount.load(std::memory_order_relaxed);
            // lostLastRef = refCount == 0;
        }
    }

} // namespace Crowny

CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::AudioClip, "AudioClip")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::Font, "Font")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::ScriptCode, "ScriptCode")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::Shader, "Shader")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::Texture, "Texture")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::VulkanTexture, "VulkanTexture")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::PhysicsMaterial2D, "PhysicsMaterial2D")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::Mesh, "Mesh")
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::AudioClip)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Shader)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Font)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Texture)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::ScriptCode)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::PhysicsMaterial2D)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Mesh)
CEREAL_REGISTER_DYNAMIC_INIT(AssetManager)