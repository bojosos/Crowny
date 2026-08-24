#pragma once

#include "Crowny/Common/HashedString.h"
#include "Crowny/Common/Module.h"
#include "Crowny/Common/StdHeaders.h"

namespace Crowny
{
    /**
     * @brief A string ID that is interned in a global table.
     *
     * StringID is a lightweight wrapper around an integer ID. Comparisons between
     * StringIDs are very fast (integer comparison). The actual string data is stored
     * only once in a global table.
     */
    class StringID
    {
    public:
        StringID() : m_ID(0) {}
        StringID(const char* str);
        StringID(const String& str);

        const char* c_str() const;
        bool IsEmpty() const { return m_ID == 0; }

        bool operator==(const StringID& rhs) const { return m_ID == rhs.m_ID; }
        bool operator!=(const StringID& rhs) const { return m_ID != rhs.m_ID; }
        bool operator<(const StringID& rhs) const { return m_ID < rhs.m_ID; }

        bool operator==(const char* rhs) const { return strcmp(c_str(), rhs) == 0; }
        bool operator!=(const char* rhs) const { return strcmp(c_str(), rhs) != 0; }

        size_t GetHash() const { return std::hash<uint32_t>{}(m_ID); }

    private:
        uint32_t m_ID;
    };

    /**
     * @brief Global table for interning strings.
     */
    class StringIDTable : public Module<StringIDTable>
    {
    public:
        StringIDTable();
        ~StringIDTable() = default;

        static uint32_t Intern(const char* str);
        static uint32_t Intern(StringView str);
        static const char* GetString(uint32_t id);
        static size_t GetEntryCount() noexcept;

    private:
        static constexpr uint32_t ENTRIES_PER_CHUNK = 256;
        static constexpr uint32_t MAX_CHUNKS = 4096;
        static constexpr uint32_t MAX_ENTRIES = ENTRIES_PER_CHUNK * MAX_CHUNKS;

        struct EntryChunk
        {
            Array<String, ENTRIES_PER_CHUNK> Entries;
        };

        struct Storage;

        static Storage& GetStorage();
        static String& GetEntry(Storage& storage, uint32_t id);
        static const String& GetEntry(const Storage& storage, uint32_t id);
    };

    namespace Detail
    {
        template <size_t N> struct StringIDLiteral
        {
            char Value[N]{};

            constexpr StringIDLiteral(const char (&value)[N]) noexcept
            {
                for (size_t i = 0; i < N; i++)
                    Value[i] = value[i];
            }
        };
    } // namespace Detail

    namespace Literals
    {
        /** Returns a cached StringID for a compile-time string literal. */
        template <Detail::StringIDLiteral Literal> StringID operator""_sid()
        {
            static const StringID id(Literal.Value);
            return id;
        }
    } // namespace Literals

} // namespace Crowny

namespace std
{
    template <> struct hash<Crowny::StringID>
    {
        size_t operator()(const Crowny::StringID& id) const { return id.GetHash(); }
    };
} // namespace std
