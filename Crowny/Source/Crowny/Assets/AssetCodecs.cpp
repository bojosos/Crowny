#include "cwpch.h"

#include "Crowny/Serialization/CerealDataStreamArchive.h"

#include "Crowny/Animation/AnimationClip.h"
#include "Crowny/Animation/Skeleton.h"
#include "Crowny/Assets/Asset.h"
#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/FileSystem.h"

#include "Crowny/Audio/AudioMixer.h"
#include "Crowny/Audio/AudioSource.h"
#include "Crowny/NodeGraph/Connection.h"
#include "Crowny/NodeGraph/Node.h"
#include "Crowny/NodeGraph/NodeGraphAsset.h"
#include "Crowny/NodeGraph/NodeRegistry.h"
#include "Crowny/NodeGraph/Pin.h"
#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/RenderAPI/Buffer.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/Shader.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/MSDFdata.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/Mesh.h"

#include "Crowny/Renderer/EnvironmentMap.h"

#include "Platform/OpenGL/OpenGLTexture.h"
#include "Platform/Vulkan/VulkanTexture.h"

#include "Crowny/Utils/Compression.h"

#include <tracy/Tracy.hpp>

#include <cereal/types/array.hpp>
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
    namespace
    {
        void InstallBackendTextureInputBindings()
        {
            static bool installed = false;
            if (installed)
                return;

            using BindingMap = cereal::detail::InputBindingMap<BinaryDataStreamInputArchive>;

            BindingMap& bindingMap = cereal::detail::StaticObject<BindingMap>::getInstance();
            auto lock = cereal::detail::StaticObject<BindingMap>::lock();

            const auto vulkanBinding = bindingMap.map.find("VulkanTexture");
            const auto openGLBinding = bindingMap.map.find("OpenGLTexture");
            if (vulkanBinding == bindingMap.map.end() || openGLBinding == bindingMap.map.end())
                throw cereal::Exception("Texture archive bindings were not registered");

            const BindingMap::Serializers vulkanSerializers = vulkanBinding->second;
            const BindingMap::Serializers openGLSerializers = openGLBinding->second;

            BindingMap::Serializers backendSerializers;
            backendSerializers.shared_ptr = [vulkan = vulkanSerializers.shared_ptr, openGL = openGLSerializers.shared_ptr](
                                              void* archive, std::shared_ptr<void>& pointer, const std::type_info& baseType) {
                switch (RenderAPI::GetAPI())
                {
                case RenderAPI::API::Vulkan:
                    vulkan(archive, pointer, baseType);
                    break;
                case RenderAPI::API::OpenGL:
                    openGL(archive, pointer, baseType);
                    break;
                default:
                    throw cereal::Exception("Cannot deserialize a texture before a render API is initialized");
                }
            };
            backendSerializers.unique_ptr = [vulkan = vulkanSerializers.unique_ptr, openGL = openGLSerializers.unique_ptr](
                                              void* archive, std::unique_ptr<void, cereal::detail::EmptyDeleter<void>>& pointer,
                                              const std::type_info& baseType) {
                switch (RenderAPI::GetAPI())
                {
                case RenderAPI::API::Vulkan:
                    vulkan(archive, pointer, baseType);
                    break;
                case RenderAPI::API::OpenGL:
                    openGL(archive, pointer, baseType);
                    break;
                default:
                    throw cereal::Exception("Cannot deserialize a texture before a render API is initialized");
                }
            };

            vulkanBinding->second = backendSerializers;
            openGLBinding->second = std::move(backendSerializers);
            installed = true;
        }
    } // namespace

    void InitializeAssetCodecs() { InstallBackendTextureInputBindings(); }

    static int64_t GetCurrentTimestamp()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    static int64_t GetFileTimestamp(const Path& path)
    {
        if (!fs::exists(path))
            return 0;
        const auto ftime = fs::last_write_time(path);
        const auto duration = ftime.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    }

    static void WriteAssetHeader(BinaryDataStreamOutputArchive& archive, AssetType type, uint32_t version, int64_t sourceTimestamp = 0,
                                 uint64_t sourceContentHash = 0)
    {
        AssetFileHeader header;
        header.Magic = ASSET_FILE_MAGIC;
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

        // Peek the magic number first. Old-format files do not have this header.
        const auto stream = archive.GetStream();
        const size_t startPos = stream->Tell();

        uint32_t magic = 0;
        if (stream->Read(&magic, sizeof(magic)) != sizeof(magic))
        {
            stream->Seek(startPos);
            return header;
        }

        if (magic != ASSET_FILE_MAGIC)
        {
            // Old format â€” rewind and return a default header so the caller
            // can proceed with legacy deserialization (no header in file)
            stream->Seek(startPos);
            CW_ENGINE_WARN("Asset file missing CWNY header â€” old format. Delete .asset files and restart to recompile.");
            return header;
        }

        // Valid header â€” read the rest
        archive(header.Version, header.Type, header.SourceTimestamp, header.CompileTimestamp, header.SourceContentHash);
        header.Magic = magic;
        return header;
    }

    static void ValidateAssetHeader(const AssetFileHeader& header, AssetType expectedType, uint32_t expectedVersion)
    {
        if (header.Magic != ASSET_FILE_MAGIC)
            return;
        if (header.Type != expectedType)
            throw cereal::Exception("Asset type does not match the serialized payload.");
        if (header.Version != expectedVersion)
            throw cereal::Exception("Asset format version is not supported. Reimport the source asset.");
    }

    bool PeekAssetHeader(const Path& assetPath, AssetFileHeader& outHeader)
    {
        outHeader = {};
        const Ref<DataStream> stream = FileSystem::OpenFile(assetPath);
        if (!stream)
            return false;

        Array<uint8_t, 4096> prefix{};
        const size_t bytesRead = stream->Read(prefix.data(), prefix.size());
        stream->Close();

        constexpr size_t SERIALIZED_HEADER_SIZE = sizeof(uint32_t) * 2u + sizeof(AssetType) + sizeof(int64_t) * 2u + sizeof(uint64_t);
        for (size_t index = 0; index + SERIALIZED_HEADER_SIZE <= bytesRead; ++index)
        {
            AssetFileHeader candidate;
            size_t cursor = index;
            std::memcpy(&candidate.Magic, prefix.data() + cursor, sizeof(candidate.Magic));
            if (candidate.Magic != ASSET_FILE_MAGIC)
                continue;

            cursor += sizeof(candidate.Magic);
            std::memcpy(&candidate.Version, prefix.data() + cursor, sizeof(candidate.Version));
            cursor += sizeof(candidate.Version);
            std::memcpy(&candidate.Type, prefix.data() + cursor, sizeof(candidate.Type));
            cursor += sizeof(candidate.Type);
            std::memcpy(&candidate.SourceTimestamp, prefix.data() + cursor, sizeof(candidate.SourceTimestamp));
            cursor += sizeof(candidate.SourceTimestamp);
            std::memcpy(&candidate.CompileTimestamp, prefix.data() + cursor, sizeof(candidate.CompileTimestamp));
            cursor += sizeof(candidate.CompileTimestamp);
            std::memcpy(&candidate.SourceContentHash, prefix.data() + cursor, sizeof(candidate.SourceContentHash));
            outHeader = candidate;
            return true;
        }
        return false;
    }

    void Save(BinaryDataStreamOutputArchive& archive, const Asset& asset) { archive(asset.m_KeepData, asset.m_Name); }

    void Load(BinaryDataStreamInputArchive& archive, Asset& asset) { archive(asset.m_KeepData, asset.m_Name); }

    void Load(BinaryDataStreamInputArchive& archive, AudioClip& clip)
    {
        const AssetFileHeader header = ReadAssetHeader(archive);
        ValidateAssetHeader(header, AssetType::AudioClip, AUDIO_FORMAT_VERSION);
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
        const auto sourceStream = clip.GetSourceStream(size);
        Vector<uint8_t> samples(size);
        sourceStream->Read(samples.data(), size); // Save the stream data
        archive(size);
        archive(cereal::binary_data(samples.data(), size));
    }

    void Save(BinaryDataStreamOutputArchive& archive, const Font& font)
    {
        if (font.m_MSDFData == nullptr || font.m_AtlasTexture == nullptr)
            throw cereal::Exception("Cannot serialize an invalid font asset.");
        WriteAssetHeader(archive, AssetType::Font, FONT_FORMAT_VERSION, font.GetSourceTimestamp(), font.GetSourceContentHash());
        archive(cereal::base_class<Asset>(&font));
        archive(font.m_MSDFData->FontGeometry);
        const msdfgen::FontMetrics& metrics = font.m_MSDFData->FontGeometry.getMetrics();
        archive(metrics.emSize, metrics.underlineY, metrics.underlineThickness, metrics.strikethroughY);
        archive(font.m_TabWidth);
        archive(font.m_AtlasPixelRange);
        archive(font.m_AtlasTexture);
    }

    void Load(BinaryDataStreamInputArchive& archive, Font& font)
    {
        const AssetFileHeader header = ReadAssetHeader(archive);
        if (header.Magic == ASSET_FILE_MAGIC && (header.Type != AssetType::Font || (header.Version != 2 && header.Version != FONT_FORMAT_VERSION)))
            throw cereal::Exception("Font asset format is not supported. Reimport the source font.");
        archive(cereal::base_class<Asset>(&font));
        font.m_MSDFData = CreateScope<MSDFData>();
        archive(font.m_MSDFData->FontGeometry);
        if (header.Magic == ASSET_FILE_MAGIC && header.Version >= 3)
        {
            msdfgen::FontMetrics& metrics = font.m_MSDFData->FontGeometry.metrics;
            archive(metrics.emSize, metrics.underlineY, metrics.underlineThickness, metrics.strikethroughY);
        }
        archive(font.m_TabWidth);
        font.m_TabWidth = std::clamp(font.m_TabWidth, 1U, 64U);
        if (header.Magic == ASSET_FILE_MAGIC && header.Version >= 3)
            archive(font.m_AtlasPixelRange);
        else
            font.m_AtlasPixelRange = 2.0f;
        if (!std::isfinite(font.m_AtlasPixelRange) || font.m_AtlasPixelRange <= 0.0f)
            throw cereal::Exception("Font atlas pixel range is invalid. Reimport the source font.");
        archive(font.m_AtlasTexture);
        font.m_FallbackFonts.clear();
    }

    void Load(BinaryDataStreamInputArchive& archive, Texture& texture)
    {
        const AssetFileHeader header = ReadAssetHeader(archive);
        if (header.Magic == ASSET_FILE_MAGIC)
        {
            if (header.Type != AssetType::Texture)
                throw cereal::Exception("Asset type does not match the serialized texture payload.");
            if (header.Version != 1 && header.Version != TEXTURE_FORMAT_VERSION)
                throw cereal::Exception("Texture asset format version is not supported. Reimport the source asset.");
        }
        archive(cereal::base_class<Asset>(&texture));
        TextureDesc& params = texture.m_Desc;
        archive(params.Type, params.Shape, params.sRGB, params.ReadWrite, params.GenerateMipmaps, params.MipLevels, params.Samples, params.Faces,
                params.Width, params.Height, params.Depth, params.Usage, params.Format);

        if (header.Magic == ASSET_FILE_MAGIC && header.Version >= 2)
        {
            archive(texture.m_DiskFormat, texture.m_SourceFormat);
            if (texture.m_DiskFormat >= TextureDiskFormat::Count)
                throw cereal::Exception("Texture asset contains an invalid disk format.");
            uint64_t encodedSize = 0;
            archive(encodedSize);
            const Ref<DataStream> stream = archive.GetStream();
            if (stream->Tell() > stream->Size() || encodedSize > stream->Size() - stream->Tell() || encodedSize > std::numeric_limits<size_t>::max())
                throw cereal::Exception("Texture asset contains a truncated encoded payload.");
            texture.m_EncodedSourceData.resize(static_cast<size_t>(encodedSize));
            if (encodedSize != 0)
                archive(cereal::binary_data(texture.m_EncodedSourceData.data(), static_cast<size_t>(encodedSize)));
            if (texture.m_DiskFormat != TextureDiskFormat::None)
            {
                if (texture.m_EncodedSourceData.empty())
                    throw cereal::Exception("Compressed texture asset has no Basis payload.");
                texture.Init();
                return;
            }
        }
        texture.Init();

        for (uint32_t mip = 0; mip < params.MipLevels + 1; mip++)
        {
            for (uint32_t face = 0; face < params.Faces; face++)
            {
                const Ref<PixelData> pixelData = texture.AllocatePixelData(face, mip);
                archive(cereal::binary_data((uint8_t*)pixelData->GetData(), pixelData->GetSize()));
                texture.WriteData(*pixelData, mip, face);
            }
        }
    }

    void Save(BinaryDataStreamOutputArchive& archive, const Texture& texture)
    {
        WriteAssetHeader(archive, AssetType::Texture, TEXTURE_FORMAT_VERSION, texture.GetSourceTimestamp(), texture.GetSourceContentHash());
        Texture& texture2 = const_cast<Texture&>(texture);

        archive(cereal::base_class<Asset>(&texture2));
        TextureDesc params = texture2.GetDesc();
        const bool encoded = texture2.HasEncodedSourceData();
        if (encoded)
            params.Format = texture2.GetSourceFormat();
        archive(params.Type, params.Shape, params.sRGB, params.ReadWrite, params.GenerateMipmaps, params.MipLevels, params.Samples, params.Faces,
                params.Width, params.Height, params.Depth, params.Usage, params.Format);
        const TextureDiskFormat diskFormat = encoded ? texture2.GetDiskFormat() : TextureDiskFormat::None;
        const TextureFormat sourceFormat = encoded ? texture2.GetSourceFormat() : params.Format;
        archive(diskFormat, sourceFormat);
        const uint64_t encodedSize = encoded ? texture2.GetEncodedSourceData().size() : 0u;
        archive(encodedSize);
        if (encoded)
        {
            archive(cereal::binary_data(texture2.GetEncodedSourceData().data(), static_cast<size_t>(encodedSize)));
            return;
        }
        for (uint32_t mip = 0; mip < params.MipLevels + 1; mip++) // Save all texture data
        {
            for (uint32_t face = 0; face < params.Faces; face++)
            {
                const Ref<PixelData> pixelData = texture2.AllocatePixelData(face, mip);
                texture2.ReadData(*pixelData, mip, face);
                archive(cereal::binary_data((uint8_t*)pixelData->GetData(),
                                            pixelData->GetSize())); // TODO: Save more pixel data (wat does this mean,
                                                                    // maybe pixel data serializer?)?
            }
        }
    }

    void Save(BinaryDataStreamOutputArchive& archive, const VulkanTexture& texture) { archive(cereal::base_class<Texture>(&texture)); }

    void Load(BinaryDataStreamInputArchive& archive, VulkanTexture& texture) { archive(cereal::base_class<Texture>(&texture)); }

    void Save(BinaryDataStreamOutputArchive& archive, const OpenGLTexture& texture) { archive(cereal::base_class<Texture>(&texture)); }

    void Load(BinaryDataStreamInputArchive& archive, OpenGLTexture& texture) { archive(cereal::base_class<Texture>(&texture)); }

    template <typename T> void SaveAnimationCurve(BinaryDataStreamOutputArchive& archive, const AnimationCurve<T>& curve)
    {
        archive(static_cast<uint32_t>(curve.GetKeyFrameCount()));
        for (const KeyFrame<T>& key : curve.GetKeyFrames())
        {
            archive(key.Time, key.Value, key.InTangent, key.OutTangent);
            archive(static_cast<uint8_t>(key.Interpolation));
        }
    }

    template <typename T> AnimationCurve<T> LoadAnimationCurve(BinaryDataStreamInputArchive& archive)
    {
        uint32_t count = 0;
        archive(count);
        Vector<KeyFrame<T>> keys(count);
        for (KeyFrame<T>& key : keys)
        {
            uint8_t interpolation = 0;
            archive(key.Time, key.Value, key.InTangent, key.OutTangent, interpolation);
            key.Interpolation = static_cast<AnimationInterpolation>(interpolation);
        }
        return AnimationCurve<T>(std::move(keys));
    }

    void Load(BinaryDataStreamInputArchive& archive, AnimationClip& clip)
    {
        const AssetFileHeader header = ReadAssetHeader(archive);
        ValidateAssetHeader(header, AssetType::AnimationClip, ANIMATION_CLIP_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&clip));
        archive(clip.m_SampleRate, clip.m_Length, clip.m_Additive);

        clip.m_RootMotion.Position = LoadAnimationCurve<glm::vec3>(archive);
        clip.m_RootMotion.Rotation = LoadAnimationCurve<glm::quat>(archive);

        uint32_t count = 0;
        archive(count);
        clip.m_TransformTracks.resize(count);
        for (AnimationTransformTrack& track : clip.m_TransformTracks)
        {
            archive(track.Name);
            track.Position = LoadAnimationCurve<glm::vec3>(archive);
            track.Rotation = LoadAnimationCurve<glm::quat>(archive);
            track.Scale = LoadAnimationCurve<glm::vec3>(archive);
        }
        archive(count);
        clip.m_MorphTracks.resize(count);
        for (AnimationMorphTrack& track : clip.m_MorphTracks)
        {
            archive(track.Name);
            track.Weight = LoadAnimationCurve<float>(archive);
        }
        archive(count);
        clip.m_GenericTracks.resize(count);
        for (AnimationGenericTrack& track : clip.m_GenericTracks)
        {
            archive(track.Name);
            track.Curve = LoadAnimationCurve<float>(archive);
        }
        archive(count);
        clip.m_Events.resize(count);
        for (AnimationEvent& event : clip.m_Events)
            archive(event.Name, event.Time, event.Payload);
        clip.SetSampleRate(clip.m_SampleRate);
        clip.RebuildLookup();
        clip.RecalculateLength();
    }

    void Save(BinaryDataStreamOutputArchive& archive, const AnimationClip& constClip)
    {
        WriteAssetHeader(archive, AssetType::AnimationClip, ANIMATION_CLIP_FORMAT_VERSION);
        AnimationClip& clip = const_cast<AnimationClip&>(constClip);
        archive(cereal::base_class<Asset>(&clip));
        archive(clip.m_SampleRate, clip.m_Length, clip.m_Additive);
        SaveAnimationCurve(archive, clip.m_RootMotion.Position);
        SaveAnimationCurve(archive, clip.m_RootMotion.Rotation);

        archive(static_cast<uint32_t>(clip.m_TransformTracks.size()));
        for (const AnimationTransformTrack& track : clip.m_TransformTracks)
        {
            archive(track.Name);
            SaveAnimationCurve(archive, track.Position);
            SaveAnimationCurve(archive, track.Rotation);
            SaveAnimationCurve(archive, track.Scale);
        }
        archive(static_cast<uint32_t>(clip.m_MorphTracks.size()));
        for (const AnimationMorphTrack& track : clip.m_MorphTracks)
        {
            archive(track.Name);
            SaveAnimationCurve(archive, track.Weight);
        }
        archive(static_cast<uint32_t>(clip.m_GenericTracks.size()));
        for (const AnimationGenericTrack& track : clip.m_GenericTracks)
        {
            archive(track.Name);
            SaveAnimationCurve(archive, track.Curve);
        }
        archive(static_cast<uint32_t>(clip.m_Events.size()));
        for (const AnimationEvent& event : clip.m_Events)
            archive(event.Name, event.Time, event.Payload);
    }

    static void SaveMeshMorph(BinaryDataStreamOutputArchive& archive, const Ref<MeshMorph>& morph)
    {
        const bool present = morph != nullptr;
        archive(present);
        if (!present)
            return;
        archive(morph->GetVertexCount(), morph->GetChannelCount());
        for (const Ref<MorphChannel>& channel : morph->GetChannels())
        {
            archive(channel->GetName(), channel->GetShapeCount());
            for (const Ref<MorphShape>& shape : channel->GetShapes())
            {
                archive(shape->GetName(), shape->GetWeight(), static_cast<uint32_t>(shape->GetVertices().size()));
                for (const MorphData& vertex : shape->GetVertices())
                    archive(vertex.VertexTranslation, vertex.NormalTranslation, vertex.VertexIndex);
            }
        }
    }

    static Ref<MeshMorph> LoadMeshMorph(BinaryDataStreamInputArchive& archive)
    {
        bool present = false;
        archive(present);
        if (!present)
            return nullptr;
        uint32_t vertexCount = 0;
        uint32_t channelCount = 0;
        archive(vertexCount, channelCount);
        Vector<Ref<MorphChannel>> channels;
        channels.reserve(channelCount);
        for (uint32_t channelIndex = 0; channelIndex < channelCount; ++channelIndex)
        {
            String channelName;
            uint32_t shapeCount = 0;
            archive(channelName, shapeCount);
            Vector<Ref<MorphShape>> shapes;
            shapes.reserve(shapeCount);
            for (uint32_t shapeIndex = 0; shapeIndex < shapeCount; ++shapeIndex)
            {
                String shapeName;
                float weight = 0.0f;
                uint32_t morphCount = 0;
                archive(shapeName, weight, morphCount);
                Vector<MorphData> morphs(morphCount);
                for (MorphData& vertex : morphs)
                    archive(vertex.VertexTranslation, vertex.NormalTranslation, vertex.VertexIndex);
                shapes.push_back(MorphShape::Create(std::move(shapeName), weight, std::move(morphs)));
            }
            channels.push_back(MorphChannel::Create(std::move(channelName), std::move(shapes)));
        }
        return MeshMorph::Create(std::move(channels), vertexCount);
    }

    static void SaveSkeleton(BinaryDataStreamOutputArchive& archive, const Ref<Skeleton>& skeleton)
    {
        const bool present = skeleton != nullptr;
        archive(present);
        if (!present)
            return;
        archive(skeleton->GetBoneCount());
        for (const SkeletonBone& bone : skeleton->GetBones())
        {
            const glm::vec3 position = bone.LocalBindPose.GetPosition();
            const glm::quat rotation = bone.LocalBindPose.GetRotation();
            const glm::vec3 scale = bone.LocalBindPose.GetScale();
            archive(bone.Name, bone.ParentIdx, position, rotation, scale, bone.InverseBindPose);
        }
    }

    static Ref<Skeleton> LoadSkeleton(BinaryDataStreamInputArchive& archive)
    {
        bool present = false;
        archive(present);
        if (!present)
            return nullptr;
        uint32_t boneCount = 0;
        archive(boneCount);
        Vector<SkeletonBone> bones(boneCount);
        for (SkeletonBone& bone : bones)
        {
            glm::vec3 position;
            glm::quat rotation;
            glm::vec3 scale;
            archive(bone.Name, bone.ParentIdx, position, rotation, scale, bone.InverseBindPose);
            bone.LocalBindPose = Transform(position, rotation, scale);
        }
        return Skeleton::Create(std::move(bones));
    }

    template <typename Archive> void Serialize(Archive& archive, Meshlet& meshlet)
    {
        archive(meshlet.VertexOffset, meshlet.TriangleOffset, meshlet.VertexCount, meshlet.TriangleCount, meshlet.MaterialSlot, meshlet.LodError,
                meshlet.BoundingSphere, meshlet.NormalCone);
    }

    template <typename Archive> void Serialize(Archive& archive, MeshLodSubMesh& subMesh)
    {
        archive(subMesh.IndexOffset, subMesh.IndexCount, subMesh.MaterialSlot);
    }

    template <typename Archive> void Serialize(Archive& archive, MeshLod& lod)
    {
        archive(lod.FirstSubMesh, lod.SubMeshCount, lod.FirstMeshlet, lod.MeshletCount, lod.Error);
    }

    void Load(BinaryDataStreamInputArchive& archive, Mesh& mesh)
    {
        const AssetFileHeader header = ReadAssetHeader(archive);
        if (header.Magic == ASSET_FILE_MAGIC)
        {
            if (header.Type != AssetType::Mesh)
                throw cereal::Exception("Asset type does not match the serialized mesh payload.");
            if (header.Version != 2 && header.Version != 3 && header.Version != MESH_FORMAT_VERSION)
                throw cereal::Exception("Mesh asset format version is not supported. Reimport the source asset.");
        }
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
        uint32_t subMeshCount = 0;
        archive(subMeshCount);
        mesh.m_SubMeshes.clear();
        mesh.m_SubMeshes.reserve(subMeshCount);
        for (uint32_t index = 0; index < subMeshCount; index++)
        {
            uint32_t offset = 0;
            uint32_t count = 0;
            DrawMode drawMode = DrawMode::TRIANGLE_LIST;
            archive(offset, count, drawMode);
            mesh.m_SubMeshes.emplace_back(offset, count, drawMode);
        }
        if (header.Magic == ASSET_FILE_MAGIC && header.Version >= 3)
        {
            archive(mesh.m_GpuGeometry.LodIndices, mesh.m_GpuGeometry.LodSubMeshes, mesh.m_GpuGeometry.Lods, mesh.m_GpuGeometry.Meshlets,
                    mesh.m_GpuGeometry.MeshletVertices, mesh.m_GpuGeometry.MeshletTriangles);
            if (header.Version >= 4)
            {
                archive(mesh.m_GpuGeometry.MeshletIndices);
            }
            else
            {
                mesh.m_GpuGeometry.MeshletIndices.resize(mesh.m_GpuGeometry.MeshletTriangles.size());
                for (const Meshlet& meshlet : mesh.m_GpuGeometry.Meshlets)
                {
                    for (uint32_t index = 0; index < meshlet.TriangleCount * 3u; index++)
                    {
                        const uint32_t triangleIndex = meshlet.TriangleOffset + index;
                        if (triangleIndex >= mesh.m_GpuGeometry.MeshletTriangles.size())
                            throw cereal::Exception("Meshlet triangle range is invalid. Reimport the source asset.");
                        const uint32_t localVertex = mesh.m_GpuGeometry.MeshletTriangles[triangleIndex];
                        if (localVertex >= meshlet.VertexCount || meshlet.VertexOffset + localVertex >= mesh.m_GpuGeometry.MeshletVertices.size())
                            throw cereal::Exception("Meshlet vertex range is invalid. Reimport the source asset.");
                        mesh.m_GpuGeometry.MeshletIndices[triangleIndex] = mesh.m_GpuGeometry.MeshletVertices[meshlet.VertexOffset + localVertex];
                    }
                }
            }
        }
        else
        {
            MeshLod lod;
            lod.FirstSubMesh = 0;
            lod.SubMeshCount = static_cast<uint32_t>(mesh.m_SubMeshes.size());
            for (uint32_t materialSlot = 0; materialSlot < mesh.m_SubMeshes.size(); materialSlot++)
            {
                const SubMesh& subMesh = mesh.m_SubMeshes[materialSlot];
                mesh.m_GpuGeometry.LodSubMeshes.push_back({ subMesh.IndexOffset, subMesh.IndexCount, materialSlot });
            }
            mesh.m_GpuGeometry.Lods.push_back(lod);
        }
        mesh.m_MeshMorph = LoadMeshMorph(archive);
        mesh.m_Skeleton = LoadSkeleton(archive);
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
        archive(static_cast<uint32_t>(mesh.m_SubMeshes.size()));
        for (const SubMesh& subMesh : mesh.m_SubMeshes)
            archive(subMesh.IndexOffset, subMesh.IndexCount, subMesh.MeshDrawMode);
        archive(mesh.m_GpuGeometry.LodIndices, mesh.m_GpuGeometry.LodSubMeshes, mesh.m_GpuGeometry.Lods, mesh.m_GpuGeometry.Meshlets,
                mesh.m_GpuGeometry.MeshletVertices, mesh.m_GpuGeometry.MeshletTriangles);
        archive(mesh.m_GpuGeometry.MeshletIndices);
        SaveMeshMorph(archive, mesh.m_MeshMorph);
        SaveSkeleton(archive, mesh.m_Skeleton);
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
        const AssetFileHeader header = ReadAssetHeader(archive);
        ValidateAssetHeader(header, AssetType::ScriptCode, 1);
        archive(cereal::base_class<Asset>(&code));
        archive(code.m_Source);
    }

    template <typename Archive> void Serialize(Archive& archive, TextureImportOptions& importOptions)
    {
        archive(importOptions.AutomaticFormat, importOptions.CpuCached, importOptions.Format, importOptions.GenerateMips, importOptions.MaxMip,
                importOptions.Shape, importOptions.SRGB, importOptions.DiskFormat, importOptions.MipFilter, importOptions.MipMode,
                importOptions.MipWrap, importOptions.PreserveAlphaCoverage, importOptions.AlphaCutoff);
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
        archive(desc.Uniforms, desc.Samplers, desc.Textures, desc.Buffers, desc.LoadStoreTextures, desc.AccelerationStructures, desc.Annotations);
    }

    void Load(BinaryDataStreamInputArchive& archive, PhysicsMaterial2D& material)
    {
        const AssetFileHeader header = ReadAssetHeader(archive);
        ValidateAssetHeader(header, AssetType::PhysicsMaterial2D, PHYSICS_MATERIAL_FORMAT_VERSION);
        if (header.Magic == ASSET_FILE_MAGIC)
            archive(cereal::base_class<Asset>(&material));

        PhysicsMaterialData data;
        archive(data.Density, data.Friction, data.Restitution, data.RestitutionThreshold);
        if (header.Magic == ASSET_FILE_MAGIC && header.Version >= 2)
        {
            uint8_t frictionCombine = 0;
            uint8_t restitutionCombine = 0;
            archive(frictionCombine, restitutionCombine);
            data.FrictionCombine = static_cast<PhysicsCombineMode>(frictionCombine);
            data.RestitutionCombine = static_cast<PhysicsCombineMode>(restitutionCombine);
        }
        material.m_Data = NormalizePhysicsMaterialData(data);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const PhysicsMaterial2D& material)
    {
        WriteAssetHeader(archive, AssetType::PhysicsMaterial2D, PHYSICS_MATERIAL_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&material));
        const PhysicsMaterialData& data = material.m_Data;
        archive(data.Density, data.Friction, data.Restitution, data.RestitutionThreshold, static_cast<uint8_t>(data.FrictionCombine),
                static_cast<uint8_t>(data.RestitutionCombine));
    }

    void Load(BinaryDataStreamInputArchive& archive, PhysicsMaterial3D& material)
    {
        const AssetFileHeader header = ReadAssetHeader(archive);
        ValidateAssetHeader(header, AssetType::PhysicsMaterial, PHYSICS_MATERIAL_FORMAT_VERSION);
        if (header.Magic != ASSET_FILE_MAGIC)
            throw cereal::Exception("A 3D physics material requires a versioned asset header.");
        archive(cereal::base_class<Asset>(&material));

        PhysicsMaterialData data;
        uint8_t frictionCombine = 0;
        uint8_t restitutionCombine = 0;
        archive(data.Density, data.Friction, data.Restitution, data.RestitutionThreshold, frictionCombine, restitutionCombine);
        data.FrictionCombine = static_cast<PhysicsCombineMode>(frictionCombine);
        data.RestitutionCombine = static_cast<PhysicsCombineMode>(restitutionCombine);
        material.m_Data = NormalizePhysicsMaterialData(data);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const PhysicsMaterial3D& material)
    {
        WriteAssetHeader(archive, AssetType::PhysicsMaterial, PHYSICS_MATERIAL_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&material));
        const PhysicsMaterialData& data = material.m_Data;
        archive(data.Density, data.Friction, data.Restitution, data.RestitutionThreshold, static_cast<uint8_t>(data.FrictionCombine),
                static_cast<uint8_t>(data.RestitutionCombine));
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
        archive(stateDesc.EnableBlending, stateDesc.AlphaToCoverage, stateDesc.SrcBlend, stateDesc.DstBlend, stateDesc.BlendOp,
                stateDesc.SrcBlendAlpha, stateDesc.DstBlendAlpha, stateDesc.BlendOpAlpha);
    }

    template <typename Archive> void Serialize(Archive& archive, RasterizerStateDesc& stateDesc)
    {
        archive(stateDesc.CullMode, stateDesc.DepthBias, stateDesc.DepthBiasSlope, stateDesc.DepthBiasClamp, stateDesc.PolygonDrawMode,
                stateDesc.DepthClipEnable, stateDesc.ScissorsEnabled);
    }

    template <typename Archive> void Serialize(Archive& archive, DepthStencilStateDesc& stateDesc)
    {
        archive(stateDesc.EnableDepthRead, stateDesc.EnableDepthWrite, stateDesc.DepthCompareFunction, stateDesc.EnableStencil,
                stateDesc.StencilReadMask, stateDesc.StencilWriteMask, stateDesc.StencilFrontCompare, stateDesc.StencilFrontFailOp,
                stateDesc.StencilFrontDepthFailOp, stateDesc.StencilFrontPassOp, stateDesc.StencilBackCompare, stateDesc.StencilBackFailOp,
                stateDesc.StencilBackDepthFailOp, stateDesc.StencilBackPassOp);
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
        const AssetFileHeader header = ReadAssetHeader(archive);
        ValidateAssetHeader(header, AssetType::Shader, SHADER_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&shader));
        archive(shader.m_Techniques);
        shader.RebuildTechniqueLookup();
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
        const UUID shaderUuid = material.m_Shader ? material.m_Shader.GetUUID() : UUID::EMPTY;
        archive(shaderUuid);
        // Per-param serialization: save each binding by name, type, and value bytes.
        // This makes .asset files resilient to uniform layout changes (reordering, additions, removals).
        const uint32_t paramCount = (uint32_t)material.m_Bindings.size();
        archive(paramCount);
        for (const auto& [paramName, member] : material.m_Bindings)
        {
            const uint32_t typeVal = (uint32_t)member.DataType;
            const uint32_t byteSize = ShaderDataTypeSize(member.DataType);
            archive(paramName, typeVal, byteSize);
            // Read the value from the first pass that contains the block
            bool written = false;
            for (const auto& pass : material.m_Passes)
            {
                const auto blockIt = pass.UniformBlocks.find(member.BufferName);
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
        const AssetFileHeader header = ReadAssetHeader(archive);
        ValidateAssetHeader(header, AssetType::Material, MATERIAL_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&material));
        UUID shaderUuid;
        archive(shaderUuid);
        if (!shaderUuid.Empty())
        {
            material.m_Shader = static_asset_cast<Shader>(AssetManager::TryGet()->LoadFromUUID(shaderUuid));
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

            const auto bindingIt = material.m_Bindings.find(paramName);
            if (bindingIt == material.m_Bindings.end())
            {
                CW_ENGINE_WARN("Material '{}': saved param '{}' not found in current shader. Discarded.", material.GetName(), paramName);
                continue;
            }
            const Material::UniformMember& member = bindingIt->second;
            const uint32_t currentSize = ShaderDataTypeSize(member.DataType);
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
            const auto v = std::get<glm::vec2>(value);
            archive(v.x, v.y);
            break;
        }
        case PinDataType::Vec3: {
            const auto v = std::get<glm::vec3>(value);
            archive(v.x, v.y, v.z);
            break;
        }
        case PinDataType::Vec4: {
            const auto v = std::get<glm::vec4>(value);
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
        const Ref<NodeGraph> graph = asset.m_Graph;
        const bool hasGraph = graph != nullptr;
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
            const auto pos = node->GetEditorPosition();
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
        const AssetFileHeader header = ReadAssetHeader(archive);
        ValidateAssetHeader(header, AssetType::NodeGraph, NODEGRAPH_FORMAT_VERSION);
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
                node->SetEditorPosition(glm::vec2(posX, posY));

            uint32_t inputPinCount, outputPinCount;
            archive(inputPinCount, outputPinCount);
            for (uint32_t p = 0; p < inputPinCount; p++)
            {
                UUID pinId;
                String pinName;
                archive(pinId, pinName);
                const PinValue value = DeserializePinValue(archive);
                if (node)
                {
                    Pin* pin = node->FindInputPin(pinName);
                    if (pin)
                    {
                        pin->SetID(pinId);
                        pin->SetDefaultValue(value);
                    }
                }
            }
            for (uint32_t p = 0; p < outputPinCount; p++)
            {
                UUID pinId;
                String pinName;
                archive(pinId, pinName);
                if (node)
                {
                    Pin* pin = node->FindOutputPin(pinName);
                    if (pin)
                        pin->SetID(pinId);
                }
            }

            if (node)
                graph->AddNode(node);
        }

        // Connections
        uint32_t connCount;
        archive(connCount);
        for (uint32_t i = 0; i < connCount; i++)
        {
            Connection conn;
            archive(conn.ID, conn.OutputNodeID, conn.OutputPinID, conn.InputNodeID, conn.InputPinID);
            graph->ConnectByPinID(conn.OutputPinID, conn.InputPinID, conn.ID);
        }

        asset.m_Graph = graph;
    }

    // ---- EnvironmentMap Serialization ----

    void Save(BinaryDataStreamOutputArchive& archive, const EnvironmentMap& envMap)
    {
        WriteAssetHeader(archive, AssetType::EnvironmentMap, ENVIRONMENT_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&envMap));
        const auto& settings = envMap.GetSettings();
        archive(settings.CubemapResolution, settings.IrradianceResolution, settings.PrefilteredResolution, settings.PrefilterSamples);
        archive(envMap.m_DiffuseSh, envMap.m_EnvironmentCubemap, envMap.m_IrradianceMap, envMap.m_PrefilteredMap);
    }

    void Load(BinaryDataStreamInputArchive& archive, EnvironmentMap& envMap)
    {
        const AssetFileHeader header = ReadAssetHeader(archive);
        if (header.Magic == ASSET_FILE_MAGIC)
        {
            if (header.Type != AssetType::EnvironmentMap)
                throw cereal::Exception("Asset type does not match the serialized environment map payload.");
            if (header.Version != 1 && header.Version != ENVIRONMENT_FORMAT_VERSION)
                throw cereal::Exception("Environment map asset format is not supported. Reimport the source asset.");
        }
        archive(cereal::base_class<Asset>(&envMap));
        EnvironmentMap::Settings settings;
        archive(settings.CubemapResolution, settings.IrradianceResolution, settings.PrefilteredResolution, settings.PrefilterSamples);
        envMap.m_Settings = settings;
        if (header.Magic == ASSET_FILE_MAGIC && header.Version >= 2)
            archive(envMap.m_DiffuseSh, envMap.m_EnvironmentCubemap, envMap.m_IrradianceMap, envMap.m_PrefilteredMap);
    }

    // ---- AudioMixer Serialization ----

    template <typename Archive> void Serialize(Archive& archive, ReverbParams& p)
    {
        archive(p.Density, p.Diffusion, p.Gain, p.GainHF, p.DecayTime, p.DecayHFRatio, p.ReflectionsGain, p.ReflectionsDelay, p.LateReverbGain,
                p.LateReverbDelay, p.AirAbsorptionGainHF, p.RoomRolloffFactor);
    }

    template <typename Archive> void Serialize(Archive& archive, EchoParams& p) { archive(p.Delay, p.LRDelay, p.Damping, p.Feedback, p.Spread); }

    template <typename Archive> void Serialize(Archive& archive, DistortionParams& p)
    {
        archive(p.Edge, p.Gain, p.LowpassCutoff, p.EqCenter, p.EqBandwidth);
    }

    template <typename Archive> void Serialize(Archive& archive, ChorusParams& p)
    {
        archive(p.Waveform, p.Phase, p.Rate, p.Depth, p.Feedback, p.Delay);
    }

    template <typename Archive> void Serialize(Archive& archive, EqualizerParams& p)
    {
        archive(p.LowGain, p.LowCutoff, p.Mid1Gain, p.Mid1Center, p.Mid1Width, p.Mid2Gain, p.Mid2Center, p.Mid2Width, p.HighGain, p.HighCutoff);
    }

    template <typename Archive> void Serialize(Archive& archive, PitchShifterParams& p) { archive(p.CoarseTune, p.FineTune); }

    template <typename Archive> void Serialize(Archive& archive, FlangerParams& p)
    {
        archive(p.Waveform, p.Phase, p.Rate, p.Depth, p.Feedback, p.Delay);
    }

    template <typename Archive> void Serialize(Archive& archive, CompressorParams& p) { archive(p.Enabled); }

    template <typename Archive> void Serialize(Archive& archive, RingModulatorParams& p) { archive(p.Frequency, p.HighpassCutoff, p.Waveform); }

    template <typename Archive> void Serialize(Archive& archive, AudioBusDesc& desc)
    {
        archive(desc.Name, desc.Parent, desc.Volume, desc.Muted, desc.FirstEffect);
        Serialize(archive, desc.Reverb);
        Serialize(archive, desc.Echo);
        Serialize(archive, desc.Distortion);
        Serialize(archive, desc.Chorus);
        Serialize(archive, desc.Equalizer);
        Serialize(archive, desc.PitchShifter);
        Serialize(archive, desc.Flanger);
        Serialize(archive, desc.Compressor);
        Serialize(archive, desc.RingModulator);
    }

    void Save(BinaryDataStreamOutputArchive& archive, const AudioMixer& mixer)
    {
        WriteAssetHeader(archive, AssetType::AudioMixer, AUDIO_MIXER_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&mixer));
        const Vector<AudioBusDesc>& descs = mixer.GetBusDescs();
        archive((uint32_t)descs.size());
        for (const AudioBusDesc& desc : descs)
            Serialize(archive, const_cast<AudioBusDesc&>(desc));
    }

    void Load(BinaryDataStreamInputArchive& archive, AudioMixer& mixer)
    {
        const AssetFileHeader header = ReadAssetHeader(archive);
        ValidateAssetHeader(header, AssetType::AudioMixer, AUDIO_MIXER_FORMAT_VERSION);
        archive(cereal::base_class<Asset>(&mixer));
        Vector<AudioBusDesc>& descs = mixer.GetBusDescs();
        descs.clear();
        uint32_t count = 0;
        archive(count);
        descs.resize(count);
        for (uint32_t i = 0; i < count; i++)
            Serialize(archive, descs[i]);
        mixer.Init();
    }

} // namespace Crowny

CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::AudioClip, "AudioClip")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::Font, "Font")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::ScriptCode, "ScriptCode")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::Shader, "Shader")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::Texture, "Texture")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::VulkanTexture, "VulkanTexture")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::OpenGLTexture, "OpenGLTexture")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::PhysicsMaterial2D, "PhysicsMaterial2D")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::PhysicsMaterial3D, "PhysicsMaterial3D")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::Mesh, "Mesh")
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::Material, "Material")
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::AudioClip)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Shader)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Font)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Texture)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::ScriptCode)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::PhysicsMaterial2D)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::PhysicsMaterial3D)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Mesh)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::Material)
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::NodeGraphAsset, "NodeGraphAsset")
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::NodeGraphAsset)
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::EnvironmentMap, "EnvironmentMap")
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::EnvironmentMap)
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::AudioMixer, "AudioMixer")
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::AudioMixer)
CEREAL_REGISTER_TYPE_WITH_NAME(Crowny::AnimationClip, "AnimationClip")
CEREAL_REGISTER_POLYMORPHIC_RELATION(Crowny::Asset, Crowny::AnimationClip)
CEREAL_REGISTER_DYNAMIC_INIT(AssetCodecs)
