#include "cwpch.h"

#include "Crowny/Common/StringID.h"

#include <stdexcept>

namespace Crowny
{
    struct StringIDTable::Storage
    {
        Storage()
        {
            for (std::atomic<EntryChunk*>& chunk : Chunks)
                chunk.store(nullptr, std::memory_order_relaxed);
        }

        ~Storage()
        {
            for (std::atomic<EntryChunk*>& chunk : Chunks)
                delete chunk.load(std::memory_order_relaxed);
        }

        UnorderedMap<uint64_t, Vector<uint32_t>> HashToIDs;
        Array<std::atomic<EntryChunk*>, MAX_CHUNKS> Chunks;
        std::atomic<uint32_t> EntryCount{ 1 };
        Mutex WriteMutex;
    };

    StringID::StringID(const char* str)
    {
        if (str == nullptr || str[0] == '\0')
        {
            m_ID = 0;
            return;
        }
        m_ID = StringIDTable::Intern(str);
    }

    StringID::StringID(const String& str)
    {
        if (str.empty())
        {
            m_ID = 0;
            return;
        }
        m_ID = StringIDTable::Intern(StringView(str));
    }

    const char* StringID::c_str() const
    {
        if (m_ID == 0)
            return "";
        return StringIDTable::GetString(m_ID);
    }

    StringIDTable::StringIDTable() { GetStorage(); }

    uint32_t StringIDTable::Intern(const char* str)
    {
        if (str == nullptr)
            return 0;
        return Intern(StringView(str));
    }

    uint32_t StringIDTable::Intern(StringView str)
    {
        if (str.empty())
            return 0;

        Storage& storage = GetStorage();
        const uint64_t hash = Hashing::CityHash64(str);
        ScopedLock lock(storage.WriteMutex);

        auto bucketIt = storage.HashToIDs.find(hash);
        if (bucketIt != storage.HashToIDs.end())
        {
            for (uint32_t id : bucketIt->second)
            {
                if (StringView(GetEntry(storage, id)) == str)
                    return id;
            }
        }

        const uint32_t id = storage.EntryCount.load(std::memory_order_relaxed);
        if (id > MAX_ENTRIES)
            throw std::overflow_error("StringID table capacity exceeded");

        const uint32_t storageIndex = id - 1;
        const uint32_t chunkIndex = storageIndex / ENTRIES_PER_CHUNK;
        const uint32_t entryIndex = storageIndex % ENTRIES_PER_CHUNK;

        EntryChunk* chunk = storage.Chunks[chunkIndex].load(std::memory_order_relaxed);
        Scope<EntryChunk> newChunk;
        if (chunk == nullptr)
        {
            newChunk = CreateScope<EntryChunk>();
            chunk = newChunk.get();
        }

        auto [newBucketIt, insertedBucket] = storage.HashToIDs.try_emplace(hash);
        try
        {
            chunk->Entries[entryIndex].assign(str.data(), str.size());
            newBucketIt->second.push_back(id);
        }
        catch (...)
        {
            if (insertedBucket && newBucketIt->second.empty())
                storage.HashToIDs.erase(newBucketIt);
            throw;
        }

        if (newChunk != nullptr)
            storage.Chunks[chunkIndex].store(newChunk.release(), std::memory_order_release);

        storage.EntryCount.store(id + 1, std::memory_order_release);
        return id;
    }

    const char* StringIDTable::GetString(uint32_t id)
    {
        if (id == 0)
            return "";

        const Storage& storage = GetStorage();
        const uint32_t entryCount = storage.EntryCount.load(std::memory_order_acquire);
        if (id >= entryCount)
            return "";

        return GetEntry(storage, id).c_str();
    }

    size_t StringIDTable::GetEntryCount() noexcept { return static_cast<size_t>(GetStorage().EntryCount.load(std::memory_order_acquire) - 1); }

    StringIDTable::Storage& StringIDTable::GetStorage()
    {
        static Storage storage;
        return storage;
    }

    String& StringIDTable::GetEntry(Storage& storage, uint32_t id)
    {
        const uint32_t storageIndex = id - 1;
        const uint32_t chunkIndex = storageIndex / ENTRIES_PER_CHUNK;
        const uint32_t entryIndex = storageIndex % ENTRIES_PER_CHUNK;
        EntryChunk* chunk = storage.Chunks[chunkIndex].load(std::memory_order_acquire);
        return chunk->Entries[entryIndex];
    }

    const String& StringIDTable::GetEntry(const Storage& storage, uint32_t id)
    {
        const uint32_t storageIndex = id - 1;
        const uint32_t chunkIndex = storageIndex / ENTRIES_PER_CHUNK;
        const uint32_t entryIndex = storageIndex % ENTRIES_PER_CHUNK;
        EntryChunk* chunk = storage.Chunks[chunkIndex].load(std::memory_order_acquire);
        return chunk->Entries[entryIndex];
    }

} // namespace Crowny
