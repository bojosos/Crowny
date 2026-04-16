#pragma once

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

        uint32_t Intern(const char* str);
        const char* GetString(uint32_t id);

    private:
        UnorderedMap<String, uint32_t> m_StringToID;
        Vector<String> m_IDToString;
        Mutex m_Mutex;
    };

} // namespace Crowny

namespace std
{
    template <> struct hash<Crowny::StringID>
    {
        size_t operator()(const Crowny::StringID& id) const { return id.GetHash(); }
    };
} // namespace std
