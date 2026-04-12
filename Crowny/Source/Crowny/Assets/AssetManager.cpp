#include "cwpch.h"

#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"

#include "Crowny/Audio/AudioSource.h"
#include "Crowny/NodeGraph/Connection.h"
#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/NodeGraphAsset.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Pin.h"
#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/MSDFdata.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/Mesh.h"

#include "Crowny/Renderer/EnvironmentMap.h"

#include "Platform/Vulkan/VulkanTexture.h"

#include "Crowny/Utils/Compression.h"

#include <tracy/Tracy.hpp>

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

    AssetManager* gAssetManager = nullptr;

    void AssetManager::OnStartUp() { gAssetManager = this; }

    void AssetManager::OnShutdown() { gAssetManager = nullptr; }

    static int64_t GetCurrentTimestamp()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    static int64_t GetFileTimestamp(const Path& path)
    {
        if (!fs::exists(path))
            return 0;
        auto ftime = fs::last_write_time(path);
        auto duration = ftime.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    }

    static void WriteAssetHeader(BinaryDataStreamOutputArchive& archive, AssetType type, uint32_t version, int64_t sourceTimestamp = 0,
                                 uint64_t sourceContentHash = 0)
    {
        AssetFileHeader header;
        header.Version = version;
        header.Type = type;
        header.SourceTimestamp = sourceTimestamp;
        header.CompileTimestamp = GetCurrentTimestamp();
        header.SourceContentHash = sourceContentHash;
        // Write magic as raw bytes so ReadAssetHeader's raw peek matches exactly
        archive.GetStream()->Write(&header.Magic, sizeof(header.Magic));
        archive(header.Version, header.Type, header.SourceTimestamp, header.CompileTimestamp, header.SourceContentHash);
    }

    static AssetFileHeader ReadAssetHeader(BinaryDataStreamInputArchive& archive)
    {
        AssetFileHeader header;

        // Peek the magic number first — old-format files won't have our header
        auto& stream = archive.GetStream();
        size_t startPos = stream->Tell();

        uint32_t magic = 0;
        stream->Read(&magic, sizeof(magic));

        if (magic != ASSET_FILE_MAGIC)
        {
            // Old format — rewind and return a default header so the caller
            // can proceed with legacy deserialization (no header in file)
            stream->Seek(startPos);
            CW_ENGINE_WARN("Asset file missing CWNY header — old format. Delete .asset files and restart to recompile.");
            return header;
        }

        // Valid header — read the rest
        archive(header.Version, header.Type, header.SourceTimestamp, header.CompileTimestamp, header.SourceContentHash);
        header.Magic = magic;
        return header;
    }

    // Reads only the header from an asset file without deserializing the full asset.
    // Useful for staleness checks without loading the entire asset.
    static bool PeekAssetHeader(const Path& assetPath, AssetFileHeader& outHeader)
    {
        Ref<DataStream> stream = FileSystem::OpenFile(assetPath);
        if (!stream)
            return false;
        BinaryDataStreamInputArchive archive(stream);
        outHeader = ReadAssetHeader(archive);
        stream->Close();
        return outHeader.Magic == ASSET_FILE_MAGIC;
    }

    void Save(BinaryDataStreamOutputArchive& archive, const Asset& asset) { archive(asset.m_KeepData, asset.m_Name); }

    void Load(BinaryDataStreamInputArchive& archive, Asset& asset) { archive(asset.m_KeepData, asset.m_Name); }

    void Load(BinaryDataStreamInputArchive& archive, AudioClip& clip)
    {
        AssetFileHeader header = ReadAssetHeader(archive);
        archive(cereal::base_class<Asset>(&clip));
        AudioClipDesc& desc = clip.m_Desc; // Save clip desc
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
        WriteAssetHeader(archive, AssetType::AudioClip, AUDIO_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&clip));
        const AudioClipDesc& desc = clip.m_Desc; // Save clip desc
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
        WriteAssetHeader(archive, AssetType::Font, FONT_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&font));
        const MSDFData* const data = font.GetMSDFData();
        archive(data->FontGeometry);
        archive(data->Glyphs);
        archive(font.m_AtlasTexture);
    }

    void Load(BinaryDataStreamInputArchive& archive, Font& font)
    {
        AssetFileHeader header = ReadAssetHeader(archive);
        archive(cereal::base_class<Asset>(&font));
        font.m_MSDFData = new MSDFData();
        archive(font.m_MSDFData->FontGeometry);
        archive(font.m_MSDFData->Glyphs);
        archive(font.m_AtlasTexture);
    }

    void Load(BinaryDataStreamInputArchive& archive, Texture& texture)
    {
        AssetFileHeader header = ReadAssetHeader(archive);
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
        WriteAssetHeader(archive, AssetType::Texture, TEXTURE_FORMAT_VERSION);
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
        AssetFileHeader header = ReadAssetHeader(archive);
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
        WriteAssetHeader(archive, AssetType::Mesh, MESH_FORMAT_VERSION);
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
        WriteAssetHeader(archive, AssetType::ScriptCode, 1);
        archive(cereal::base_class<Asset>(&code));
        archive(code.m_Source);
    }

    void Load(BinaryDataStreamInputArchive& archive, ScriptCode& code)
    {
        AssetFileHeader header = ReadAssetHeader(archive);
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

    void Save(BinaryDataStreamOutputArchive& archive, const BufferLayout& layout) { archive(layout.m_Id, layout.m_Elements); }

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
        AssetFileHeader header = ReadAssetHeader(archive);
        archive(cereal::base_class<Asset>(&shader));
        archive(shader.m_Techniques);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const Shader& shader)
    {
        WriteAssetHeader(archive, AssetType::Shader, SHADER_FORMAT_VERSION, shader.m_SourceTimestamp, shader.m_SourceContentHash);
        archive(cereal::base_class<Asset>(&shader));
        archive(shader.m_Techniques);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const Material& material)
    {
        WriteAssetHeader(archive, AssetType::Material, MATERIAL_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&material));
        // Shader reference
        UUID shaderUuid = material.m_Shader ? material.m_Shader.GetUUID() : UUID::EMPTY;
        archive(shaderUuid);
        // Per-param serialization: save each binding by name, type, and value bytes.
        // This makes .asset files resilient to uniform layout changes (reordering, additions, removals).
        uint32_t paramCount = (uint32_t)material.m_Bindings.size();
        archive(paramCount);
        for (const auto& [paramName, member] : material.m_Bindings)
        {
            uint32_t typeVal = (uint32_t)member.DataType;
            uint32_t byteSize = ShaderDataTypeSize(member.DataType);
            archive(paramName, typeVal, byteSize);
            // Read the value from the first pass that contains the block
            bool written = false;
            for (const auto& pass : material.m_Passes)
            {
                auto blockIt = pass.UniformBlocks.find(member.BufferName);
                if (blockIt != pass.UniformBlocks.end() && member.Offset + byteSize <= blockIt->second->m_Size)
                {
                    Vector<uint8_t> buf(byteSize, 0);
                    blockIt->second->Read(member.Offset, buf.data(), byteSize);
                    archive(cereal::binary_data(buf.data(), byteSize));
                    written = true;
                    break;
                }
            }
            if (!written)
            {
                Vector<uint8_t> zeroBuf(byteSize, 0);
                archive(cereal::binary_data(zeroBuf.data(), byteSize));
            }
        }
    }

    void Load(BinaryDataStreamInputArchive& archive, Material& material)
    {
        AssetFileHeader header = ReadAssetHeader(archive);
        archive(cereal::base_class<Asset>(&material));
        UUID shaderUuid;
        archive(shaderUuid);
        if (!shaderUuid.Empty())
        {
            material.m_Shader = static_asset_cast<Shader>(gAssetManager->LoadFromUUID(shaderUuid));
            if (material.m_Shader)
                material.ReloadParams();
        }
        uint32_t paramCount;
        archive(paramCount);
        for (uint32_t i = 0; i < paramCount; i++)
        {
            String paramName;
            uint32_t typeVal, byteSize;
            archive(paramName, typeVal, byteSize);
            Vector<uint8_t> buf(byteSize);
            archive(cereal::binary_data(buf.data(), byteSize));

            auto bindingIt = material.m_Bindings.find(paramName);
            if (bindingIt == material.m_Bindings.end())
            {
                CW_ENGINE_WARN("Material '{}': saved param '{}' not found in current shader. Discarded.", material.GetName(), paramName);
                continue;
            }
            const Material::UniformMember& member = bindingIt->second;
            uint32_t currentSize = ShaderDataTypeSize(member.DataType);
            if ((uint32_t)member.DataType != typeVal || currentSize != byteSize)
            {
                CW_ENGINE_WARN("Material '{}': param '{}' type/size mismatch (saved type={} size={}, current type={} size={}). Discarded.",
                               material.GetName(), paramName, typeVal, byteSize, (uint32_t)member.DataType, currentSize);
                continue;
            }
            for (auto& pass : material.m_Passes)
            {
                auto blockIt = pass.UniformBlocks.find(member.BufferName);
                if (blockIt != pass.UniformBlocks.end() && member.Offset + byteSize <= blockIt->second->m_Size)
                    blockIt->second->Write(member.Offset, buf.data(), byteSize);
            }
        }
    }

    AssetHandle<Asset> AssetManager::Load(const Path& filepath, bool keepInternalRef, bool keepSourceData)
    {
        ZoneScopedN("AssetManager::Load");
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

    void AssetManager::Save(const AssetHandle<Asset>& asset, const Path& filepath, bool overwrite)
    {
        if (!asset)
            return;

        if (fs::exists(filepath) && !overwrite)
        {
            CW_ENGINE_ERROR("File exists, not saving");
            return;
        }

        Save(asset.GetInternalPtr(), filepath);
    }

    void AssetManager::Save(const Ref<Asset>& asset, const Path& filepath)
    {
        if (!fs::is_directory(filepath.parent_path()))
            fs::create_directories(filepath.parent_path());
        Ref<DataStream> stream = FileSystem::CreateAndOpenFile(filepath);
        BinaryDataStreamOutputArchive archive(stream);
        archive(asset);
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

    bool AssetManager::GetAssetPath(const UUID& uuid, Path& outPath) const
    {
        for (const auto& manifest : m_Manifests)
        {
            if (manifest->UuidToFilepath(uuid, outPath))
                return true;
        }
        return false;
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
        for (auto& manifest : m_Manifests)
        {
            if (manifest->FilepathToUuid(filepath, outUUID))
                return true;
        }
        // No manifest has this filepath registered — caller will generate a new UUID.
        // This happens for assets loaded directly by path that weren't imported through the ProjectLibrary.
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
        m_Handles[uuid] = newHandle.GetWeak();
        return newHandle;
    }

    void AssetManager::Release(AssetHandleBase& handle)
    {
        const UUID& uuid = handle.GetUUID();
        m_Handles.erase(uuid);
    }

    // ---- NodeGraph Serialization ----

    static void SerializePinValue(BinaryDataStreamOutputArchive& archive, PinDataType type, const PinValue& value)
    {
        archive((uint32_t)type);
        switch (type)
        {
        case PinDataType::Float:
            archive(std::get<float>(value));
            break;
        case PinDataType::Int:
            archive(std::get<int32_t>(value));
            break;
        case PinDataType::Vec2: {
            auto v = std::get<glm::vec2>(value);
            archive(v.x, v.y);
            break;
        }
        case PinDataType::Vec3: {
            auto v = std::get<glm::vec3>(value);
            archive(v.x, v.y, v.z);
            break;
        }
        case PinDataType::Vec4: {
            auto v = std::get<glm::vec4>(value);
            archive(v.x, v.y, v.z, v.w);
            break;
        }
        case PinDataType::Bool:
            archive(std::get<bool>(value));
            break;
        default:
            break;
        }
    }

    static PinValue DeserializePinValue(BinaryDataStreamInputArchive& archive)
    {
        uint32_t type;
        archive(type);
        switch ((PinDataType)type)
        {
        case PinDataType::Float: {
            float v;
            archive(v);
            return v;
        }
        case PinDataType::Int: {
            int32_t v;
            archive(v);
            return v;
        }
        case PinDataType::Vec2: {
            float x, y;
            archive(x, y);
            return glm::vec2(x, y);
        }
        case PinDataType::Vec3: {
            float x, y, z;
            archive(x, y, z);
            return glm::vec3(x, y, z);
        }
        case PinDataType::Vec4: {
            float x, y, z, w;
            archive(x, y, z, w);
            return glm::vec4(x, y, z, w);
        }
        case PinDataType::Bool: {
            bool v;
            archive(v);
            return v;
        }
        default:
            return 0.0f;
        }
    }

    void Save(BinaryDataStreamOutputArchive& archive, const NodeGraphAsset& asset)
    {
        WriteAssetHeader(archive, AssetType::NodeGraph, NODEGRAPH_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&asset));
        Ref<NodeGraph> graph = asset.m_Graph;
        bool hasGraph = graph != nullptr;
        archive(hasGraph);
        if (!hasGraph)
            return;

        // Graph metadata
        archive(graph->GetID(), graph->GetName(), graph->GetDomain());

        // Nodes
        const auto& nodes = graph->GetNodes();
        archive((uint32_t)nodes.size());
        for (const auto& [id, node] : nodes)
        {
            archive(node->GetTypeName(), node->GetID());
            auto pos = node->GetEditorPosition();
            archive(pos.x, pos.y);

            // Pins (Input + Output)
            const auto& inputs = node->GetInputPins();
            const auto& outputs = node->GetOutputPins();
            archive((uint32_t)inputs.size(), (uint32_t)outputs.size());
            for (const auto& pin : inputs)
            {
                archive(pin->GetID(), pin->GetName());
                SerializePinValue(archive, pin->GetDataType(), pin->GetDefaultValue());
            }
            for (const auto& pin : outputs)
            {
                archive(pin->GetID(), pin->GetName());
            }
        }

        // Connections
        const auto& connections = graph->GetConnections();
        archive((uint32_t)connections.size());
        for (const auto& conn : connections)
            archive(conn.ID, conn.OutputNodeID, conn.OutputPinID, conn.InputNodeID, conn.InputPinID);
    }

    void Load(BinaryDataStreamInputArchive& archive, NodeGraphAsset& asset)
    {
        AssetFileHeader header = ReadAssetHeader(archive);
        archive(cereal::base_class<Asset>(&asset));
        bool hasGraph;
        archive(hasGraph);
        if (!hasGraph)
            return;

        UUID graphId;
        String graphName;
        uint32_t domain;
        archive(graphId, graphName, domain);

        auto graph = CreateRef<NodeGraph>(graphId);
        graph->SetName(graphName);
        graph->SetDomain((NodeGraph::Domain)domain);

        // Nodes
        uint32_t nodeCount;
        archive(nodeCount);
        for (uint32_t i = 0; i < nodeCount; i++)
        {
            String typeName;
            UUID nodeId;
            float posX, posY;
            archive(typeName, nodeId);
            archive(posX, posY);

            auto node = NodeRegistry::Get().Create(typeName, nodeId);
            if (node)
            {
                node->SetEditorPosition(glm::vec2(posX, posY));

                uint32_t inputPinCount, outputPinCount;
                archive(inputPinCount, outputPinCount);
                for (uint32_t p = 0; p < inputPinCount; p++)
                {
                    UUID pinId;
                    String pinName;
                    archive(pinId, pinName);
                    PinValue value = DeserializePinValue(archive);
                    Pin* pin = node->FindInputPin(pinName);
                    if (pin)
                    {
                        pin->SetID(pinId);
                        pin->SetDefaultValue(value);
                    }
                }
                for (uint32_t p = 0; p < outputPinCount; p++)
                {
                    UUID pinId;
                    String pinName;
                    archive(pinId, pinName);
                    Pin* pin = node->FindOutputPin(pinName);
                    if (pin)
                        pin->SetID(pinId);
                }

                graph->AddNode(node);
            }
        }

        // Connections
        uint32_t connCount;
        archive(connCount);
        for (uint32_t i = 0; i < connCount; i++)
        {
            Connection conn;
            archive(conn.ID, conn.OutputNodeID, conn.OutputPinID, conn.InputNodeID, conn.InputPinID);
            graph->ConnectByPinID(conn.OutputPinID, conn.InputPinID);
        }

        asset.m_Graph = graph;
    }

    // ---- EnvironmentMap Serialization ----
    // EnvironmentMap is regenerated from settings, not stored as raw textures

    void Save(BinaryDataStreamOutputArchive& archive, const EnvironmentMap& envMap)
    {
        WriteAssetHeader(archive, AssetType::EnvironmentMap, ENVIRONMENT_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&envMap));
        const auto& settings = envMap.GetSettings();
        archive(settings.CubemapResolution, settings.IrradianceResolution, settings.PrefilteredResolution, settings.PrefilterSamples);
    }

    void Load(BinaryDataStreamInputArchive& archive, EnvironmentMap& envMap)
    {
        AssetFileHeader header = ReadAssetHeader(archive);
        archive(cereal::base_class<Asset>(&envMap));
        EnvironmentMap::Settings settings;
        archive(settings.CubemapResolution, settings.IrradianceResolution, settings.PrefilteredResolution, settings.PrefilterSamples);
        // Note: IBL textures are regenerated when the source HDR is available, not stored in the asset file.
        // The caller is responsible for triggering regeneration after load.
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
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::Material, "Material")
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::AudioClip)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Shader)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Font)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Texture)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::ScriptCode)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::PhysicsMaterial2D)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Mesh)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Material)
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::NodeGraphAsset, "NodeGraphAsset")
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::NodeGraphAsset)
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::EnvironmentMap, "EnvironmentMap")
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::EnvironmentMap)
CEREAL_REGISTER_DYNAMIC_INIT(AssetManager)