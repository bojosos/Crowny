#pragma once

#include "Crowny/Common/DataStream.h"
#include "Crowny/Common/HashedString.h"
#include "Crowny/Common/Module.h"

namespace Crowny
{
    class BuiltInResourcePack final : public Module<BuiltInResourcePack>
    {
    public:
        explicit BuiltInResourcePack(const Path& packPath);

        bool IsValid() const { return m_Valid; }
        bool Contains(const Path& path) const;
        bool TryGetSize(const Path& path, uint64_t& size) const;
        Ref<DataStream> Open(const Path& path) const;
        size_t GetEntryCount() const { return m_Entries.size(); }
        const Path& GetPath() const { return m_Path; }

    private:
        struct Entry
        {
            uint64_t Offset = 0;
            uint64_t Size = 0;
        };

        String NormalizePath(const Path& path) const;
        Path GetLoosePath(const Path& path) const;
        bool HasLooseFile(const Path& path) const;
        bool Load();
        bool PreferSourceFile(const Path& path) const;
        const Entry* Find(const Path& path) const;

        Path m_Path;
        Path m_LooseRoot;
        Vector<uint8_t> m_Data;
        UnorderedMap<String, Entry, StringHash, StringEqual> m_Entries;
        fs::file_time_type m_PackWriteTime{};
        bool m_ForceSourceFiles = false;
        bool m_Valid = false;
    };
} // namespace Crowny
