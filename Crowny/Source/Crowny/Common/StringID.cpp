#include "cwpch.h"

#include "Crowny/Common/StringID.h"

namespace Crowny
{
    StringID::StringID(const char* str)
    {
        if (str == nullptr || str[0] == '\0')
        {
            m_ID = 0;
            return;
        }
        m_ID = StringIDTable::Get().Intern(str);
    }

    StringID::StringID(const String& str)
    {
        if (str.empty())
        {
            m_ID = 0;
            return;
        }
        m_ID = StringIDTable::Get().Intern(str.c_str());
    }

    const char* StringID::c_str() const
    {
        if (m_ID == 0)
            return "";
        return StringIDTable::Get().GetString(m_ID);
    }

    StringIDTable::StringIDTable()
    {
        m_IDToString.push_back(""); // ID 0 is empty string
    }

    uint32_t StringIDTable::Intern(const char* str)
    {
        ScopedLock lock(m_Mutex);
        auto it = m_StringToID.find(str);
        if (it != m_StringToID.end())
            return it->second;

        uint32_t id = (uint32_t)m_IDToString.size();
        m_IDToString.push_back(str);
        m_StringToID[str] = id;
        return id;
    }

    const char* StringIDTable::GetString(uint32_t id)
    {
        ScopedLock lock(m_Mutex);
        if (id >= m_IDToString.size())
            return "";
        return m_IDToString[id].c_str();
    }

} // namespace Crowny
