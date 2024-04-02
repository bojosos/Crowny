#pragma once

namespace Crowny
{

    class UUID
    {
    public:
        UUID() = default;
        UUID(uint32_t data1, uint32_t data2, uint32_t data3, uint32_t data4);
        UUID(const String& uuid);

        String ToString() const;

        bool operator==(const UUID& rhs) const
        {
            return m_Data[0] == rhs.m_Data[0] && m_Data[1] == rhs.m_Data[1] && m_Data[2] == rhs.m_Data[2] &&
                   m_Data[3] == rhs.m_Data[3];
        }

        bool operator!=(const UUID& rhs) const { return !(*this == rhs); }

        bool operator<(const UUID& rhs) const
        {
            for (uint32_t i = 0; i < 4; i++)
            {
                if (m_Data[i] < rhs.m_Data[i])
                    return true;
                else if (m_Data[i] > rhs.m_Data[i])
                    return false;
            }
            return false;
        }

        bool Empty() const { return m_Data[0] == 0 && m_Data[1] == 0 && m_Data[2] == 0 && m_Data[3] == 0; }

        template <typename OStream> friend OStream& operator<<(OStream& os, const UUID& ms)
        {
            return os << ms.ToString();
        }

        static UUID EMPTY;

        friend struct std::hash<UUID>;
        CW_SIMPLESERIALZABLE(UUID);

    private:
        uint32_t m_Data[4] = { 0, 0, 0, 0 };
    };

    class UuidGenerator
    {
    public:
        static UUID Generate();
    };
} // namespace Crowny

namespace std
{
    template <> struct hash<Crowny::UUID>
    {
        size_t operator()(const Crowny::UUID& uuid) const
        {
            size_t hash = 0;
            Crowny::HashCombine(hash, uuid.m_Data[0], uuid.m_Data[1], uuid.m_Data[2], uuid.m_Data[3]);
            return hash;
        }
    };
} // namespace std