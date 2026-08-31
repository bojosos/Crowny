#include "cwepch.h"

#include "Editor/AssetPreviewService.h"

#include "Crowny/Audio/AudioClip.h"
#include "Crowny/Audio/AudioDecoder.h"
#include "Crowny/Audio/OggVorbisDecoder.h"
#include "Crowny/Audio/WaveDecoder.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/StringUtils.h"
#include "Crowny/ImGui/ImGuiVulkanTexture.h"
#include "Crowny/Import/ImageLoader.h"
#include "Crowny/Import/MeshImporter.h"
#include "Crowny/Threading/TaskSystem.h"

#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/fmt/fmt.h>

#include <stdexcept>

namespace Crowny
{
    struct AssetPreviewService::WorkItem : RefCounted
    {
        UUID Uuid;
        Path Source;
        AssetType Type = AssetType::None;
        std::time_t SourceTime = 0;
        uint32_t SourceSize = 0;
        uint64_t SourceRevision = 0;
        uint32_t PreviewSize = 0;
        uint64_t LastAccess = 0;
        AssetPreviewResult Result;
        Ref<PixelData> Pixels;
        Ref<Task> TaskHandle;
        std::atomic<bool> Cancellation{ false };
    };

    namespace
    {
        Ref<PixelData> CreateCanvas(uint32_t size)
        {
            Ref<PixelData> output = PixelData::Create(size, size, 1, TextureFormat::RGBA8);
            for (uint32_t y = 0; y < size; y++)
            {
                for (uint32_t x = 0; x < size; x++)
                {
                    const bool light = ((x / 8u) + (y / 8u)) % 2u == 0u;
                    const float value = light ? 0.19f : 0.12f;
                    output->SetColorAt(x, y, glm::vec4(value, value, value, 1.0f));
                }
            }
            return output;
        }

        Ref<PixelData> MakeImagePreview(const ImageLoadResult& image, uint32_t size, const std::atomic<bool>& cancellation)
        {
            if (!image.Pixels || !image.Pixels->IsValid() || size == 0)
                return nullptr;

            Ref<PixelData> output = CreateCanvas(size);
            const float scale = std::min(static_cast<float>(size) / image.Pixels->GetWidth(), static_cast<float>(size) / image.Pixels->GetHeight());
            const uint32_t width = std::max(1u, static_cast<uint32_t>(std::round(image.Pixels->GetWidth() * scale)));
            const uint32_t height = std::max(1u, static_cast<uint32_t>(std::round(image.Pixels->GetHeight() * scale)));
            const uint32_t offsetX = (size - width) / 2u;
            const uint32_t offsetY = (size - height) / 2u;

            for (uint32_t y = 0; y < height; y++)
            {
                if (cancellation.load(std::memory_order_acquire))
                    return nullptr;
                const uint32_t sourceY =
                  std::min(static_cast<uint32_t>((static_cast<uint64_t>(y) * image.Pixels->GetHeight()) / height), image.Pixels->GetHeight() - 1u);
                for (uint32_t x = 0; x < width; x++)
                {
                    const uint32_t sourceX =
                      std::min(static_cast<uint32_t>((static_cast<uint64_t>(x) * image.Pixels->GetWidth()) / width), image.Pixels->GetWidth() - 1u);
                    glm::vec4 color;
                    if (!image.Pixels->TryGetColorAt(sourceX, sourceY, 0, color))
                        continue;
                    if (image.Info.Channels == 1)
                        color.g = color.b = color.r;
                    else if (image.Info.Channels == 2)
                        color = glm::vec4(color.r, color.r, color.r, color.g);
                    if (image.Info.IsHDR)
                    {
                        color.r = LinearToSRGB(std::max(color.r, 0.0f) / (1.0f + std::max(color.r, 0.0f)));
                        color.g = LinearToSRGB(std::max(color.g, 0.0f) / (1.0f + std::max(color.g, 0.0f)));
                        color.b = LinearToSRGB(std::max(color.b, 0.0f) / (1.0f + std::max(color.b, 0.0f)));
                    }
                    glm::vec4 background = output->GetColorAt(offsetX + x, offsetY + y);
                    const float alpha = glm::clamp(color.a, 0.0f, 1.0f);
                    color = glm::vec4(glm::mix(glm::vec3(background), glm::vec3(color), alpha), 1.0f);
                    output->SetColorAt(offsetX + x, offsetY + y, color);
                }
            }
            return output;
        }

        float DecodePcmSample(const uint8_t* sample, uint32_t bitDepth)
        {
            switch (bitDepth)
            {
            case 8:
                return static_cast<float>(static_cast<int8_t>(*sample)) / 128.0f;
            case 16: {
                int16_t value = 0;
                std::memcpy(&value, sample, sizeof(value));
                return static_cast<float>(value) / 32768.0f;
            }
            case 24: {
                int32_t value = static_cast<int32_t>(sample[0]) | static_cast<int32_t>(sample[1]) << 8 | static_cast<int32_t>(sample[2]) << 16;
                if ((value & 0x00800000) != 0)
                    value |= static_cast<int32_t>(0xFF000000);
                return static_cast<float>(value) / 8388608.0f;
            }
            case 32: {
                int32_t value = 0;
                std::memcpy(&value, sample, sizeof(value));
                return static_cast<float>(value) / 2147483648.0f;
            }
            default:
                return 0.0f;
            }
        }

        Ref<AudioDecoder> CreateAudioDecoder(StringView extension)
        {
            if (extension == ".ogg")
                return CreateRef<OggVorbisDecoder>();
            if (extension == ".wav")
                return CreateRef<WaveDecoder>();
            return nullptr;
        }

        Ref<PixelData> MakeAudioPreview(const Path& path, uint32_t size, AssetPreviewResult& result, const std::atomic<bool>& cancellation,
                                        String& error)
        {
            String extension = path.extension().string();
            StringUtils::ToLower(extension);
            Ref<AudioDecoder> decoder = CreateAudioDecoder(extension);
            const Ref<DataStream> stream = FileSystem::OpenFile(path);
            if (!decoder || !stream || !decoder->IsValid(stream))
            {
                error = "Unsupported or corrupt audio source";
                return nullptr;
            }

            AudioDataInfo info{};
            if (!decoder->Open(stream, info) || info.NumChannels == 0 || info.SampleRate == 0 || info.BitDepth % 8u != 0u)
            {
                error = "Audio metadata could not be read";
                return nullptr;
            }

            result.Channels = info.NumChannels;
            result.SampleRate = info.SampleRate;
            result.Duration = static_cast<float>(info.NumSamples) / static_cast<float>(info.NumChannels) / static_cast<float>(info.SampleRate);
            result.Details = fmt::format("{:.2f} s, {} ch, {} Hz", result.Duration, result.Channels, result.SampleRate);

            Ref<PixelData> output = PixelData::Create(size, size, 1, TextureFormat::RGBA8);
            for (uint32_t y = 0; y < size; y++)
                for (uint32_t x = 0; x < size; x++)
                    output->SetColorAt(x, y, glm::vec4(0.055f, 0.065f, 0.08f, 1.0f));

            const uint32_t bytesPerSample = info.BitDepth / 8u;
            const uint32_t maximumSamples = std::max(2048u, info.NumChannels);
            Vector<uint8_t> samples(static_cast<size_t>(maximumSamples) * bytesPerSample);
            const uint32_t center = size / 2u;
            for (uint32_t x = 0; x < size; x++)
            {
                if (cancellation.load(std::memory_order_acquire))
                    return nullptr;
                uint32_t first = static_cast<uint32_t>((static_cast<uint64_t>(info.NumSamples) * x) / size);
                uint32_t last = static_cast<uint32_t>((static_cast<uint64_t>(info.NumSamples) * (x + 1u)) / size);
                first -= first % info.NumChannels;
                last = std::max(last, first + info.NumChannels);
                const uint32_t count = std::min(last - first, maximumSamples);
                decoder->Seek(first);
                const uint32_t read = decoder->Read(samples.data(), count);
                float peak = 0.0f;
                for (uint32_t sample = 0; sample < read; sample++)
                    peak = std::max(peak, std::abs(DecodePcmSample(samples.data() + static_cast<size_t>(sample) * bytesPerSample, info.BitDepth)));

                const uint32_t amplitude = static_cast<uint32_t>(glm::clamp(peak, 0.0f, 1.0f) * (size * 0.44f));
                const uint32_t top = center > amplitude ? center - amplitude : 0u;
                const uint32_t bottom = std::min(center + amplitude, size - 1u);
                for (uint32_t y = top; y <= bottom; y++)
                    output->SetColorAt(x, y, glm::vec4(0.15f, 0.72f, 0.95f, 1.0f));
            }
            return output;
        }

        void DrawLine(PixelData& image, int32_t x0, int32_t y0, int32_t x1, int32_t y1, const glm::vec4& color)
        {
            const int32_t dx = std::abs(x1 - x0);
            const int32_t sx = x0 < x1 ? 1 : -1;
            const int32_t dy = -std::abs(y1 - y0);
            const int32_t sy = y0 < y1 ? 1 : -1;
            int32_t error = dx + dy;
            while (true)
            {
                if (x0 >= 0 && y0 >= 0 && x0 < static_cast<int32_t>(image.GetWidth()) && y0 < static_cast<int32_t>(image.GetHeight()))
                    image.SetColorAt(static_cast<uint32_t>(x0), static_cast<uint32_t>(y0), color);
                if (x0 == x1 && y0 == y1)
                    break;
                const int32_t twiceError = error * 2;
                if (twiceError >= dy)
                {
                    error += dy;
                    x0 += sx;
                }
                if (twiceError <= dx)
                {
                    error += dx;
                    y0 += sy;
                }
            }
        }

        Ref<PixelData> MakeMeshPreview(const Path& path, uint32_t size, AssetPreviewResult& result, const std::atomic<bool>& cancellation,
                                       String& error)
        {
            MeshImportOptions options;
            options.CpuCached = true;
            options.ImportMaterials = false;
            options.ImportAnimations = false;
            options.ImportMorphMeshes = false;
            options.ImportBones = false;
            options.GenerateMeshlets = false;
            options.GenerateLods = false;
            const MeshImportResult mesh = MeshImporter::Parse(path, options);
            if (!mesh || !mesh.Data)
            {
                error = "Mesh source could not be parsed";
                return nullptr;
            }

            Vector<glm::vec3> positions = mesh.Data->GetPositions();
            Vector<uint32_t> indices = mesh.Data->GetIndices();
            if (positions.empty() || indices.empty())
            {
                error = "Mesh has no renderable geometry";
                return nullptr;
            }

            const glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(-24.0f), glm::vec3(1.0f, 0.0f, 0.0f)) *
                                       glm::rotate(glm::mat4(1.0f), glm::radians(38.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::vec3 minimum(std::numeric_limits<float>::max());
            glm::vec3 maximum(std::numeric_limits<float>::lowest());
            for (glm::vec3& position : positions)
            {
                position = glm::vec3(rotation * glm::vec4(position, 1.0f));
                minimum = glm::min(minimum, position);
                maximum = glm::max(maximum, position);
            }
            const glm::vec3 center = (minimum + maximum) * 0.5f;
            const float extent = std::max(maximum.x - minimum.x, maximum.y - minimum.y);
            if (extent <= 1e-8f)
            {
                error = "Mesh bounds are degenerate";
                return nullptr;
            }
            const float scale = static_cast<float>(size) * 0.82f / extent;
            for (glm::vec3& position : positions)
            {
                position -= center;
                position.x = position.x * scale + static_cast<float>(size) * 0.5f;
                position.y = static_cast<float>(size) * 0.5f - position.y * scale;
            }

            Ref<PixelData> output = PixelData::Create(size, size, 1, TextureFormat::RGBA8);
            for (uint32_t y = 0; y < size; y++)
                for (uint32_t x = 0; x < size; x++)
                    output->SetColorAt(x, y, glm::vec4(0.045f, 0.052f, 0.065f, 1.0f));
            Vector<float> depth(static_cast<size_t>(size) * size, std::numeric_limits<float>::max());

            Vector<SubMesh> subMeshes = mesh.SubMeshes;
            if (subMeshes.empty())
                subMeshes.emplace_back(0, static_cast<uint32_t>(indices.size()), DrawMode::TRIANGLE_LIST);
            uint64_t sourceTriangleCount = 0;
            for (const SubMesh& subMesh : subMeshes)
                if (subMesh.MeshDrawMode == DrawMode::TRIANGLE_LIST)
                    sourceTriangleCount += subMesh.IndexCount / 3u;
            constexpr uint64_t MAX_PREVIEW_TRIANGLES = 100000;
            const uint64_t triangleStride = std::max<uint64_t>((sourceTriangleCount + MAX_PREVIEW_TRIANGLES - 1u) / MAX_PREVIEW_TRIANGLES, 1u);

            uint64_t visitedTriangles = 0;
            uint64_t renderedTriangles = 0;
            for (const SubMesh& subMesh : subMeshes)
            {
                if (subMesh.MeshDrawMode != DrawMode::TRIANGLE_LIST)
                    continue;
                const uint32_t end = std::min(subMesh.IndexOffset + subMesh.IndexCount, static_cast<uint32_t>(indices.size()));
                for (uint32_t index = subMesh.IndexOffset; index + 2u < end; index += 3u)
                {
                    const uint64_t candidate = visitedTriangles++;
                    if ((candidate & 1023u) == 0u && cancellation.load(std::memory_order_acquire))
                        return nullptr;
                    if (candidate % triangleStride != 0u)
                        continue;
                    const uint32_t ia = indices[index];
                    const uint32_t ib = indices[index + 1u];
                    const uint32_t ic = indices[index + 2u];
                    if (ia >= positions.size() || ib >= positions.size() || ic >= positions.size())
                        continue;
                    const glm::vec3 a = positions[ia];
                    const glm::vec3 b = positions[ib];
                    const glm::vec3 c = positions[ic];
                    const float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
                    if (std::abs(area) < 1e-8f)
                        continue;
                    const int32_t minX = glm::clamp(static_cast<int32_t>(std::floor(std::min({ a.x, b.x, c.x }))), 0, static_cast<int32_t>(size) - 1);
                    const int32_t maxX = glm::clamp(static_cast<int32_t>(std::ceil(std::max({ a.x, b.x, c.x }))), 0, static_cast<int32_t>(size) - 1);
                    const int32_t minY = glm::clamp(static_cast<int32_t>(std::floor(std::min({ a.y, b.y, c.y }))), 0, static_cast<int32_t>(size) - 1);
                    const int32_t maxY = glm::clamp(static_cast<int32_t>(std::ceil(std::max({ a.y, b.y, c.y }))), 0, static_cast<int32_t>(size) - 1);
                    const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
                    const float light = 0.25f + 0.65f * std::abs(glm::dot(normal, glm::normalize(glm::vec3(0.4f, 0.7f, 0.6f))));
                    const glm::vec4 color(0.24f * light, 0.55f * light, 0.78f * light, 1.0f);
                    for (int32_t y = minY; y <= maxY; y++)
                    {
                        for (int32_t x = minX; x <= maxX; x++)
                        {
                            const glm::vec2 point(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
                            const float w0 = ((b.x - point.x) * (c.y - point.y) - (b.y - point.y) * (c.x - point.x)) / area;
                            const float w1 = ((c.x - point.x) * (a.y - point.y) - (c.y - point.y) * (a.x - point.x)) / area;
                            const float w2 = 1.0f - w0 - w1;
                            if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
                                continue;
                            const float z = a.z * w0 + b.z * w1 + c.z * w2;
                            const size_t pixel = static_cast<size_t>(y) * size + static_cast<uint32_t>(x);
                            if (z < depth[pixel])
                            {
                                depth[pixel] = z;
                                output->SetColorAt(static_cast<uint32_t>(x), static_cast<uint32_t>(y), color);
                            }
                        }
                    }
                    DrawLine(*output, static_cast<int32_t>(a.x), static_cast<int32_t>(a.y), static_cast<int32_t>(b.x), static_cast<int32_t>(b.y),
                             glm::vec4(0.08f, 0.12f, 0.16f, 1.0f));
                    DrawLine(*output, static_cast<int32_t>(b.x), static_cast<int32_t>(b.y), static_cast<int32_t>(c.x), static_cast<int32_t>(c.y),
                             glm::vec4(0.08f, 0.12f, 0.16f, 1.0f));
                    DrawLine(*output, static_cast<int32_t>(c.x), static_cast<int32_t>(c.y), static_cast<int32_t>(a.x), static_cast<int32_t>(a.y),
                             glm::vec4(0.08f, 0.12f, 0.16f, 1.0f));
                    renderedTriangles++;
                }
            }

            if (renderedTriangles == 0)
            {
                error = "Mesh has no triangle-list submeshes";
                return nullptr;
            }
            result.Details = fmt::format("{} vertices, {} triangles", positions.size(), sourceTriangleCount);
            return output;
        }

    } // namespace

    void AssetPreviewService::ExecutePreviewWork(const Ref<WorkItem>& work)
    {
        if (work->Cancellation.load(std::memory_order_acquire))
            return;

        try
        {
            if (work->Type == AssetType::Texture || work->Type == AssetType::EnvironmentMap)
            {
                ImageLoadOptions options;
                options.FlipVertically = true;
                options.Preserve16Bit = true;
                options.Cancellation = &work->Cancellation;
                const ImageLoadResult image = ImageLoader::Decode(work->Source, options);
                if (image.Canceled)
                    work->Cancellation.store(true, std::memory_order_release);
                else if (!image)
                    work->Result.Error = image.Error;
                else
                {
                    work->Pixels = MakeImagePreview(image, work->PreviewSize, work->Cancellation);
                    work->Result.Details = fmt::format("{} x {}, {} channel{}, {}-bit{}{}", image.Info.Width, image.Info.Height,
                                                       image.Info.Channels, image.Info.Channels == 1 ? "" : "s", image.Info.BitDepth,
                                                       image.Info.IsHDR ? ", HDR" : "", image.Info.Faces == 6 ? ", cubemap" : "");
                }
            }
            else if (work->Type == AssetType::AudioClip)
                work->Pixels = MakeAudioPreview(work->Source, work->PreviewSize, work->Result, work->Cancellation, work->Result.Error);
            else if (work->Type == AssetType::Mesh || work->Type == AssetType::MeshSource)
                work->Pixels = MakeMeshPreview(work->Source, work->PreviewSize, work->Result, work->Cancellation, work->Result.Error);
        }
        catch (const std::exception& exception)
        {
            work->Result.Error = exception.what();
        }
        catch (...)
        {
            work->Result.Error = "Preview generation failed with an unknown error";
        }
    }

    void AssetPreviewService::CancelPreviewWork(const Ref<WorkItem>& work)
    {
        work->Cancellation.store(true, std::memory_order_release);
        if (work->TaskHandle)
            work->TaskHandle->Cancel();
    }

    AssetPreviewService::~AssetPreviewService() { Clear(); }

    bool AssetPreviewService::Supports(AssetType type)
    {
        return type == AssetType::Texture || type == AssetType::EnvironmentMap || type == AssetType::Mesh || type == AssetType::MeshSource ||
               type == AssetType::AudioClip;
    }

    const AssetPreviewResult* AssetPreviewService::Request(const FileEntry& entry, uint32_t size)
    {
        if (!entry.Metadata || entry.Metadata->Uuid.Empty() || !Supports(entry.Metadata->Type) || size == 0)
            return nullptr;
        size = std::min(size, MAX_PREVIEW_DIMENSION);

        const UUID uuid = entry.Metadata->Uuid;
        auto existing = m_Cache.find(uuid);
        if (existing != m_Cache.end())
        {
            Ref<WorkItem>& work = existing->second;
            if (work->Source == entry.Filepath && work->SourceTime == entry.LastUpdateTime && work->SourceSize == entry.Filesize &&
                work->SourceRevision == entry.Revision && work->PreviewSize == size)
            {
                work->LastAccess = ++m_AccessTick;
                return &work->Result;
            }
            CancelPreviewWork(work);
            ImGuiVulkanTexture::Release(work->Result.Image);
            m_Cache.erase(existing);
        }

        if (m_Pending.size() + m_Running.size() >= MAX_PENDING)
            return &m_QueueFullResult;

        Ref<WorkItem> work = CreateRef<WorkItem>();
        work->Uuid = uuid;
        work->Source = entry.Filepath;
        work->Type = entry.Metadata->Type;
        work->SourceTime = entry.LastUpdateTime;
        work->SourceSize = entry.Filesize;
        work->SourceRevision = entry.Revision;
        work->PreviewSize = size;
        work->LastAccess = ++m_AccessTick;
        work->Result.Status = AssetPreviewStatus::Queued;
        m_Cache.emplace(uuid, work);
        m_Pending.push_back(work);
        return &work->Result;
    }

    void AssetPreviewService::Update()
    {
        FinalizeCompletedWork();
        StartPendingWork();
        EvictOldEntries();
    }

    void AssetPreviewService::StartPendingWork()
    {
        while (m_Running.size() < MAX_CONCURRENT && !m_Pending.empty())
        {
            Ref<WorkItem> work = m_Pending.front();
            m_Pending.pop_front();
            if (work->Cancellation.load(std::memory_order_acquire))
                continue;
            work->Result.Status = AssetPreviewStatus::Loading;
            work->TaskHandle = Task::Create("Asset preview", [work]() { ExecutePreviewWork(work); }, TaskPriority::Low);
            m_Running.push_back(work);
            try
            {
                TaskSystem* taskSystem = TaskSystem::TryGet();
                if (taskSystem == nullptr)
                    throw std::logic_error("Task system is unavailable");
                taskSystem->Submit(work->TaskHandle);
            }
            catch (const std::exception& exception)
            {
                m_Running.pop_back();
                work->TaskHandle = nullptr;
                work->Result.Status = AssetPreviewStatus::Failed;
                work->Result.Error = exception.what();
            }
            catch (...)
            {
                m_Running.pop_back();
                work->TaskHandle = nullptr;
                work->Result.Status = AssetPreviewStatus::Failed;
                work->Result.Error = "Preview task submission failed";
            }
        }
    }

    void AssetPreviewService::FinalizeCompletedWork()
    {
        for (size_t index = 0; index < m_Running.size();)
        {
            Ref<WorkItem> work = m_Running[index];
            if (!work->TaskHandle || !work->TaskHandle->IsComplete())
            {
                index++;
                continue;
            }
            m_Running.erase(m_Running.begin() + static_cast<std::ptrdiff_t>(index));

            const auto cached = m_Cache.find(work->Uuid);
            if (work->Cancellation.load(std::memory_order_acquire) || cached == m_Cache.end() || cached->second != work)
                continue;
            if (!work->Pixels)
            {
                work->Result.Status = AssetPreviewStatus::Failed;
                if (work->Result.Error.empty())
                    work->Result.Error = "Preview generation was canceled";
                continue;
            }

            try
            {
                TextureDesc desc;
                desc.Width = work->Pixels->GetWidth();
                desc.Height = work->Pixels->GetHeight();
                desc.Format = TextureFormat::RGBA8;
                desc.Usage = TextureUsage::TEXTURE_STATIC;
                desc.sRGB = true;
                desc.DebugName = "Asset preview";
                work->Result.Image = Texture::CreateDeferred(desc, work->Pixels);
                work->Result.Image->Init();
                work->Pixels = nullptr;
                work->Result.Status = AssetPreviewStatus::Ready;
            }
            catch (const std::exception& exception)
            {
                work->Result.Image = nullptr;
                work->Result.Status = AssetPreviewStatus::Failed;
                work->Result.Error = exception.what();
            }
        }
    }

    void AssetPreviewService::CancelPending()
    {
        for (const Ref<WorkItem>& work : m_Pending)
            CancelPreviewWork(work);
        m_Pending.clear();
        for (const Ref<WorkItem>& work : m_Running)
            CancelPreviewWork(work);

        for (auto entry = m_Cache.begin(); entry != m_Cache.end();)
        {
            if (entry->second->Result.Status == AssetPreviewStatus::Ready || entry->second->Result.Status == AssetPreviewStatus::Failed)
                ++entry;
            else
                entry = m_Cache.erase(entry);
        }
    }

    void AssetPreviewService::Invalidate(const UUID& uuid)
    {
        const auto entry = m_Cache.find(uuid);
        if (entry == m_Cache.end())
            return;
        CancelPreviewWork(entry->second);
        ImGuiVulkanTexture::Release(entry->second->Result.Image);
        m_Cache.erase(entry);
    }

    void AssetPreviewService::Clear()
    {
        CancelPending();
        for (const Ref<WorkItem>& work : m_Running)
        {
            if (!work->TaskHandle)
                continue;
            try
            {
                work->TaskHandle->Wait();
            }
            catch (...)
            {
                // Clearing owns no result reporting. It only needs every worker to leave the asset and decoder modules.
            }
        }
        m_Running.clear();
        for (const auto& [uuid, work] : m_Cache)
            ImGuiVulkanTexture::Release(work->Result.Image);
        m_Cache.clear();
    }

    void AssetPreviewService::EvictOldEntries()
    {
        while (m_Cache.size() > MAX_CACHE_ENTRIES)
        {
            auto oldest = m_Cache.end();
            for (auto entry = m_Cache.begin(); entry != m_Cache.end(); ++entry)
            {
                const AssetPreviewStatus status = entry->second->Result.Status;
                if ((status == AssetPreviewStatus::Ready || status == AssetPreviewStatus::Failed) &&
                    (oldest == m_Cache.end() || entry->second->LastAccess < oldest->second->LastAccess))
                    oldest = entry;
            }
            if (oldest == m_Cache.end())
                break;
            ImGuiVulkanTexture::Release(oldest->second->Result.Image);
            m_Cache.erase(oldest);
        }
    }

} // namespace Crowny
