#include "cwpch.h"

#include "Crowny/Assets/AssetManager.h"

#include "Crowny/Assets/AssetCodecs.h"
#include "Crowny/Assets/AssetListener.h"
#include "Crowny/Common/FileSystem.h"
#include "Crowny/Physics/PhysicsMaterial.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Renderer/Font.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"
#include "Crowny/Serialization/PrefabSerializer.h"

#include <condition_variable>
#include <deque>
#include <fstream>
#include <future>
#include <thread>
#include <tracy/Tracy.hpp>

namespace Crowny
{
    namespace
    {
        class AssetWriteQueue
        {
        public:
            explicit AssetWriteQueue(size_t workers)
            {
                m_Workers.reserve(workers);
                try
                {
                    for (size_t index = 0; index < workers; ++index)
                        m_Workers.emplace_back([this] {
                            for (;;)
                            {
                                std::packaged_task<String()> job;
                                {
                                    std::unique_lock lock(m_Mutex);
                                    m_Ready.wait(lock, [this] { return m_Closed || !m_Jobs.empty(); });
                                    if (m_Jobs.empty())
                                        return;
                                    job = std::move(m_Jobs.front());
                                    m_Jobs.pop_front();
                                }
                                job();
                            }
                        });
                }
                catch (...)
                {
                    Close();
                    throw;
                }
            }

            ~AssetWriteQueue() { Close(); }

            std::shared_future<String> Submit(std::function<String()> function)
            {
                std::packaged_task<String()> job(std::move(function));
                auto result = job.get_future().share();
                {
                    std::lock_guard lock(m_Mutex);
                    m_Jobs.push_back(std::move(job));
                }
                m_Ready.notify_one();
                return result;
            }

        private:
            void Close()
            {
                {
                    std::lock_guard lock(m_Mutex);
                    m_Closed = true;
                }
                m_Ready.notify_all();
                for (auto& worker : m_Workers)
                    if (worker.joinable())
                        worker.join();
            }

            std::mutex m_Mutex;
            std::condition_variable m_Ready;
            std::deque<std::packaged_task<String()>> m_Jobs;
            Vector<std::thread> m_Workers;
            bool m_Closed = false;
        };

        bool AssetPayloadMatches(const Path& path, const byte* data, size_t size, AssetType type)
        {
            // These formats have one top-level header and no embedded asset headers.
            // Keep the original compile time when every other byte is unchanged.
            if (type != AssetType::Texture && type != AssetType::Material && type != AssetType::Mesh)
                return false;
            constexpr size_t timestampInHeader = sizeof(uint32_t) * 2 + sizeof(AssetType) + sizeof(int64_t);
            constexpr size_t headerSize = timestampInHeader + sizeof(int64_t) + sizeof(uint64_t);
            size_t timestamp = size;
            for (size_t offset = 0; offset + headerSize <= std::min(size, size_t{ 256 }); ++offset)
            {
                uint32_t magic;
                AssetType serializedType;
                std::memcpy(&magic, data + offset, sizeof(magic));
                std::memcpy(&serializedType, data + offset + sizeof(uint32_t) * 2, sizeof(serializedType));
                if (magic == ASSET_FILE_MAGIC && serializedType == type)
                {
                    timestamp = offset + timestampInHeader;
                    break;
                }
            }
            if (timestamp == size)
                return false;
            std::ifstream stream(path, std::ios::binary | std::ios::ate);
            if (!stream || stream.tellg() != static_cast<std::streamoff>(size))
                return false;
            stream.seekg(0);
            std::array<byte, 64 * 1024> buffer;
            for (size_t offset = 0; offset < size; offset += buffer.size())
            {
                const size_t count = std::min(buffer.size(), size - offset);
                if (!stream.read(reinterpret_cast<char*>(buffer.data()), count))
                    return false;
                if (offset == 0)
                    std::memcpy(buffer.data() + timestamp, data + timestamp, sizeof(int64_t));
                if (std::memcmp(buffer.data(), data + offset, count) != 0)
                    return false;
            }
            return true;
        }
    } // namespace

    void AssetManager::OnStartUp() { InitializeAssetCodecs(); }

    void AssetManager::OnShutdown()
    {
        m_Handles.clear();
        m_TransientAssetIds.clear();
        m_Manifests.clear();
    }

    AssetHandle<Asset> AssetManager::Load(const Path& filepath, bool keepInternalRef, bool keepSourceData)
    {
        ZoneScopedN("AssetManager::Load");
        const Path normalizedPath = filepath.lexically_normal();
        if (!FileSystem::FileExists(normalizedPath))
        {
            CW_ENGINE_WARN("Resource {0} does not exist or is not a regular file.", normalizedPath);
            return nullptr;
        }

        UUID uuid;
        if (!GetUUIDFromFilepath(normalizedPath, uuid))
        {
            const auto transientIter = m_TransientAssetIds.find(normalizedPath);
            if (transientIter != m_TransientAssetIds.end())
                uuid = transientIter->second;
            else
            {
                uuid = UuidGenerator::Generate();
                m_TransientAssetIds[normalizedPath] = uuid;
            }
        }
        if (uuid.Empty())
            uuid = UuidGenerator::Generate();
        return Load(uuid, normalizedPath, keepInternalRef, keepSourceData);
    }

    AssetHandle<Asset> AssetManager::LoadFromUUID(const UUID& uuid, bool keepInternalRef, bool keepSourceData)
    {
        const auto iterFind = m_Handles.find(uuid);
        if (iterFind != m_Handles.end() && iterFind->second.IsLoaded())
        {
            AssetHandle<Asset> loaded = iterFind->second.Lock();
            if (keepInternalRef)
                loaded.AddInternalRef();
            return loaded;
        }
        Path filepath;
        GetFilepathFromUUID(uuid, filepath);
        if (filepath.empty() || !FileSystem::FileExists(filepath))
        {
            return AssetHandle<Asset>();
        }
        return Load(uuid, filepath, keepInternalRef, keepSourceData);
    }

    AssetHandle<Asset> AssetManager::Load(const UUID& uuid, const Path& filepath, bool keepInternalRef, bool keepSourceData)
    {
        const auto iterFind = m_Handles.find(uuid);
        if (iterFind != m_Handles.end() && iterFind->second.IsLoaded())
        {
            AssetHandle<Asset> loaded = iterFind->second.Lock();
            if (keepInternalRef)
                loaded.AddInternalRef();
            return loaded;
        }

        Ref<Asset> asset;
        try
        {
            const Ref<DataStream> stream = FileSystem::OpenFile(filepath);
            if (stream == nullptr || !stream->IsReadable())
            {
                CW_ENGINE_ERROR("Unable to open asset '{}'.", filepath);
                return nullptr;
            }

            // Older editors copied prefab source YAML directly into the .asset cache.
            // Keep those caches readable without changing their manifest UUIDs.
            constexpr StringView prefabHeader = "# Crowny Prefab";
            std::array<char, prefabHeader.size() + 1> prefix{};
            const size_t prefixSize = stream->Read(prefix.data(), prefix.size());
            stream->Seek(0);
            if (prefixSize == prefix.size() && StringView(prefix.data(), prefabHeader.size()) == prefabHeader &&
                (prefix.back() == '\n' || prefix.back() == '\r'))
            {
                auto prefab = CreateRef<Prefab>();
                PrefabSerializer(prefab).DeserializeFromString(stream->GetAsString());
                asset = prefab;
            }
            else
            {
                BinaryDataStreamInputArchive archive(stream);
                archive(asset);
            }
            stream->Close();
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Failed to load asset '{}': {}", filepath, error.what());
            return nullptr;
        }

        if (asset == nullptr)
        {
            CW_ENGINE_ERROR("Asset '{}' contained a null payload.", filepath);
            return nullptr;
        }

        if (!keepSourceData && asset->GetAssetType() == AssetType::Texture)
        {
            const Ref<Texture> texture = StaticRefCast<Texture>(asset);
            if (!texture->IsCpuCached())
                texture->ReleaseSourceData();
        }

        AssetHandle<Asset> output;
        if (iterFind != m_Handles.end())
        {
            output = iterFind->second.Lock();
            output.SetHandleData(asset, uuid);
        }
        else
            output = AssetHandle<Asset>(asset, uuid);

        if (keepInternalRef)
            output.AddInternalRef();
        output.NotifyLoadComplete();
        m_Handles[uuid] = output.GetWeak();
        if (asset->GetAssetType() == AssetType::Font)
            StaticRefCast<Font>(asset)->LoadFallbackFonts();
        if (AssetListenerManager::IsStartedUp())
            AssetListenerManager::Get().NotifyListeners(uuid);
        if (asset->GetAssetType() == AssetType::PhysicsMaterial2D)
            StaticRefCast<PhysicsMaterial2D>(asset)->NotifyChanged();
        else if (asset->GetAssetType() == AssetType::PhysicsMaterial)
            StaticRefCast<PhysicsMaterial3D>(asset)->NotifyChanged();
        return output;
    }

    AssetHandle<Asset> AssetManager::GetAssetHandle(const UUID& uuid)
    {
        const auto iterFind = m_Handles.find(uuid);
        if (iterFind != m_Handles.end())
            return iterFind->second.Lock();
        AssetHandle<Asset> handle(uuid);
        m_Handles[uuid] = handle.GetWeak();
        return handle;
    }

    bool AssetManager::Save(const AssetHandle<Asset>& asset, const Path& filepath, bool overwrite)
    {
        if (!asset)
            return false;

        if (fs::exists(filepath) && !overwrite)
        {
            CW_ENGINE_ERROR("File exists, not saving");
            return false;
        }

        return Save(asset.GetInternalPtr(), filepath);
    }

    bool AssetManager::Save(const Ref<Asset>& asset, const Path& filepath)
    {
        if (asset == nullptr || filepath.empty())
        {
            CW_ENGINE_ERROR("Cannot save a null asset or use an empty filepath.");
            return false;
        }

        try
        {
            const Ref<MemoryDataStream> stream = CreateRef<MemoryDataStream>(4096);
            {
                BinaryDataStreamOutputArchive archive(stream);
                archive(asset);
            }
            String writeError;
            if (!FileSystem::WriteFileAtomic(filepath, stream->Data(), stream->Tell(), &writeError))
            {
                CW_ENGINE_ERROR("Unable to publish asset '{}': {}", filepath, writeError);
                return false;
            }

            NotifyAssetSaved(asset, filepath);
            return true;
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Failed to save asset '{}': {}", filepath, error.what());
            return false;
        }
    }

    void AssetManager::NotifyAssetSaved(const Ref<Asset>& asset, const Path& filepath)
    {
        UUID uuid;
        if (GetUUIDFromFilepath(filepath.lexically_normal(), uuid))
        {
            const auto handleIter = m_Handles.find(uuid);
            if (handleIter != m_Handles.end())
            {
                AssetHandle<Asset> loaded = handleIter->second.Lock();
                loaded.SetHandleData(asset, uuid);
                loaded.NotifyLoadComplete();
            }
            if (AssetListenerManager::IsStartedUp())
                AssetListenerManager::Get().NotifyListeners(uuid);
        }
    }

    bool AssetManager::SaveBatch(const Vector<std::pair<Ref<Asset>, Path>>& assets)
    {
        UnorderedSet<Path, HashPath> destinations;
        for (const auto& [asset, path] : assets)
            if (!asset || path.empty() || !destinations.insert(path.lexically_normal()).second)
                return false;

        struct PendingSave
        {
            Ref<Asset> Value;
            Path Destination;
            size_t Bytes;
            std::shared_future<String> Write;
        };
        UnorderedMap<const Asset*, Ref<MemoryDataStream>> sharedTextures;
        size_t sharedBytes = 0;
        Vector<std::pair<Ref<Asset>, Path>> notifications;
        std::deque<PendingSave> pending;
        size_t queuedBytes = 0;
        std::unique_ptr<AssetWriteQueue> writers;
        bool succeeded = true;
        const auto finish = [&] {
            PendingSave save = std::move(pending.front());
            pending.pop_front();
            queuedBytes -= save.Bytes;
            try
            {
                const String error = save.Write.get();
                if (error.empty())
                    notifications.emplace_back(save.Value, save.Destination);
                else
                {
                    CW_ENGINE_ERROR("Unable to publish asset '{}': {}", save.Destination, error);
                    succeeded = false;
                }
            }
            catch (const std::exception& error)
            {
                CW_ENGINE_ERROR("Failed to complete asset save '{}': {}", save.Destination, error.what());
                succeeded = false;
            }
        };
        try
        {
            writers = std::make_unique<AssetWriteQueue>(std::min<size_t>(16, assets.size()));
            for (const auto& [asset, path] : assets)
            {
                const auto shared = sharedTextures.find(asset.get());
                const bool reuse = shared != sharedTextures.end();
                const auto stream = reuse ? shared->second : CreateRef<MemoryDataStream>(4096);
                if (!reuse)
                {
                    BinaryDataStreamOutputArchive archive(stream);
                    archive(asset);
                }
                const size_t bytes = stream->Tell();
                while (!pending.empty() && (pending.size() >= 16 || queuedBytes + bytes > 64u * 1024u * 1024u))
                    finish();
                if (!succeeded)
                    break;
                const auto type = asset->GetAssetType();
                auto write = writers->Submit([stream, path, bytes, type] {
                    String error;
                    try
                    {
                        if (AssetPayloadMatches(path, stream->Data(), bytes, type))
                            return error;
                        if (!FileSystem::WriteFileAtomic(path, stream->Data(), bytes, &error) && error.empty())
                            error = "Atomic write failed";
                    }
                    catch (const std::exception& exception)
                    {
                        error = exception.what();
                    }
                    return error;
                });
                // Model importers can return the same texture in many material slots.
                // Preserve every dependent path/UUID while serializing that immutable
                // texture once. Bound retained source payloads independently of the queue.
                if (!reuse && asset->GetAssetType() == AssetType::Texture && sharedBytes + bytes <= 32u * 1024u * 1024u)
                {
                    sharedTextures.emplace(asset.get(), stream);
                    sharedBytes += bytes;
                }
                pending.push_back({ asset, path, bytes, write });
                queuedBytes += bytes;
            }
        }
        catch (const std::exception& error)
        {
            CW_ENGINE_ERROR("Failed to serialize asset batch: {}", error.what());
            succeeded = false;
        }
        // Wait before returning, including on failure, so callers can safely roll
        // back uncommitted cache paths without racing background publishers.
        while (!pending.empty())
            finish();
        writers.reset();
        // Notifications may mutate loaded assets. Deliver them only after the batch
        // has finished reading and publishing all of its input objects.
        for (const auto& [asset, path] : notifications)
        {
            try
            {
                NotifyAssetSaved(asset, path);
            }
            catch (const std::exception& error)
            {
                CW_ENGINE_ERROR("Failed to notify asset save '{}': {}", path, error.what());
                succeeded = false;
            }
        }
        return succeeded;
    }

    void AssetManager::RegisterAssetManifest(const Ref<AssetManifest>& manifest)
    {
        if (manifest == nullptr)
            return;
        const auto iterFind = std::find(m_Manifests.begin(), m_Manifests.end(), manifest);
        if (iterFind == m_Manifests.end())
            m_Manifests.push_back(manifest);
        else
            *iterFind = manifest;
    }

    void AssetManager::UnregisterAssetManifest(const Ref<AssetManifest>& manifest)
    {
        const auto iterFind = std::find(m_Manifests.begin(), m_Manifests.end(), manifest);
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
        outPath.clear();
        return false;
    }

    bool AssetManager::IsAssetRegistered(const UUID& uuid) const
    {
        for (const Ref<AssetManifest>& manifest : m_Manifests)
        {
            if (manifest && manifest->UuidExists(uuid))
                return true;
        }
        return false;
    }

    void AssetManager::GetFilepathFromUUID(const UUID& uuid, Path& outFilepath) const
    {
        outFilepath.clear();
        for (const auto& manifest : m_Manifests)
        {
            if (manifest->UuidToFilepath(uuid, outFilepath))
                return;
        }
    }

    bool AssetManager::GetUUIDFromFilepath(const Path& filepath, UUID& outUUID) const
    {
        for (const auto& manifest : m_Manifests)
        {
            if (manifest->FilepathToUuid(filepath, outUUID))
                return true;
        }
        // No manifest has this filepath registered â€” caller will generate a new UUID.
        // This happens for assets loaded directly by path that weren't imported through the ProjectLibrary.
        return false;
    }

    AssetHandle<Asset> AssetManager::CreateAssetHandle(const Ref<Asset>& asset)
    {
        const UUID uuid = UuidGenerator::Generate();
        return CreateAssetHandle(asset, uuid);
    }

    AssetHandle<Asset> AssetManager::CreateAssetHandle(const Ref<Asset>& asset, const UUID& uuid)
    {
        if (asset == nullptr || uuid.Empty())
            return nullptr;

        const auto existing = m_Handles.find(uuid);
        if (existing != m_Handles.end())
        {
            AssetHandle<Asset> handle = existing->second.Lock();
            handle.SetHandleData(asset, uuid);
            handle.NotifyLoadComplete();
            return handle;
        }

        const AssetHandle<Asset> newHandle(asset, uuid);
        m_Handles[uuid] = newHandle.GetWeak();
        return newHandle;
    }

    void AssetManager::Release(AssetHandleBase& handle)
    {
        const UUID uuid = handle.GetUUID();
        const Ref<AssetHandleData> data = handle.GetHandleData();
        const auto iter = m_Handles.find(uuid);
        if (iter != m_Handles.end() && iter->second.GetHandleData() == data)
            m_Handles.erase(iter);

        handle.ClearHandleData();
    }

    // ---- NodeGraph Serialization ----
} // namespace Crowny
