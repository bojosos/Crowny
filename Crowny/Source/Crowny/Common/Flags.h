#pragma once

namespace Crowny
{

    template <typename Enum, typename Storage = uint32_t> class Flags
    {
    public:
        Flags() = default;

        Flags(Enum value) { m_Bits = static_cast<Storage>(value); }

        Flags(const Flags<Enum, Storage>& value) { m_Bits = value.m_Bits; }

        explicit Flags(Storage bits) { m_Bits = bits; }

        bool IsSet(Enum value) const { return (m_Bits & static_cast<Storage>(value)) == static_cast<Storage>(value); }

        bool IsSetAny(Enum value) const { return (m_Bits & static_cast<Storage>(value)) != 0; }

        bool IsSetAny(const Flags<Enum, Storage>& value) const { return (m_Bits & value.m_Bits) != 0; }

        Flags<Enum, Storage>& Set(Enum value)
        {
            m_Bits |= static_cast<Storage>(value);
            return *this;
        }

        void Unset(Enum value) { m_Bits &= ~static_cast<Storage>(value); }

        bool operator==(Enum rhs) const { return m_Bits == static_cast<Storage>(rhs); }

        bool operator==(const Flags<Enum, Storage>& rhs) const { return m_Bits == rhs.m_Bits; }

        bool operator==(bool rhs) const { return ((bool)*this) == rhs; }

        bool operator!=(Enum rhs) const { return m_Bits != static_cast<Storage>(rhs); }

        bool operator!=(const Flags<Enum, Storage>& rhs) const { return m_Bits != rhs.m_Bits; }

        Flags<Enum, Storage>& operator=(Enum rhs)
        {
            m_Bits = static_cast<Storage>(rhs);
            return *this;
        }

        Flags<Enum, Storage>& operator=(const Flags<Enum, Storage>& rhs)
        {
            m_Bits = rhs.m_Bits;
            return *this;
        }

        Flags<Enum, Storage> operator|(Enum rhs)
        {
            Flags<Enum, Storage> out = *this;
            out |= static_cast<Storage>(rhs);
            return out;
        }

        Flags<Enum, Storage> operator|(const Flags<Enum, Storage>& rhs)
        {
            Flags<Enum, Storage> out = *this;
            out |= rhs.m_Bits;
            return out;
        }

        Flags<Enum, Storage> operator|=(Enum rhs)
        {
            m_Bits |= static_cast<Storage>(rhs);
            return *this;
        }

        Flags<Enum, Storage> operator|=(const Flags<Enum, Storage>& rhs)
        {
            m_Bits |= rhs.m_Bits;
            return *this;
        }

        Flags<Enum, Storage> operator&(Enum rhs) const
        {
            Flags<Enum, Storage> out = *this;
            out.m_Bits &= static_cast<Storage>(rhs);
            return out;
        }

        Flags<Enum, Storage> operator&(const Flags<Enum, Storage>& rhs) const
        {
            Flags<Enum, Storage> out = *this;
            out.m_Bits &= rhs.m_Bits;
            return out;
        }

        Flags<Enum, Storage>& operator&=(Enum rhs)
        {
            m_Bits &= static_cast<Storage>(rhs);
            return *this;
        }

        Flags<Enum, Storage>& operator&=(const Flags<Enum, Storage>& rhs) const
        {
            m_Bits &= rhs.m_Bits;
            return *this;
        }

        Flags<Enum, Storage> operator^(Enum rhs) const
        {
            Flags<Enum, Storage> out = *this;
            out.m_Bits ^= static_cast<Storage>(rhs);
            return out;
        }

        Flags<Enum, Storage>& operator^=(Enum rhs)
        {
            m_Bits ^= static_cast<Storage>(rhs);
            return *this;
        }

        Flags<Enum, Storage>& operator^=(const Flags<Enum, Storage>& rhs)
        {
            m_Bits ^= rhs.m_Bits;
            return *this;
        }

        operator bool() const { return m_Bits ? true : false; }

        operator uint8_t() const { return static_cast<uint8_t>(m_Bits); }

        operator uint16_t() const { return static_cast<uint16_t>(m_Bits); }

        operator uint32_t() const { return static_cast<uint32_t>(m_Bits); }

        Flags<Enum, Storage> operator^(const Flags<Enum, Storage>& rhs) const
        {
            Flags<Enum, Storage> out = *this;
            out.m_Bits ^= rhs.m_Bits;
            return out;
        }

        Flags<Enum, Storage> operator~() const
        {
            Flags<Enum, Storage> out;
            out.m_Bits = (Storage)~m_Bits;
            return out;
        }

        friend Flags<Enum, Storage> operator&(Enum a, Flags<Enum, Storage>& b)
        {
            Flags<Enum, Storage> out;
            out.m_Bits = a & b.m_Bits;
            return out;
        }

    private:
        Storage m_Bits{ 0 };
    };

#define CW_FLAGS_OPERATORS(Enum)                                                                                       \
    inline Flags<Enum, uint32_t> operator|(Enum a, Enum b)                                                             \
    {                                                                                                                  \
        Flags<Enum, uint32_t> r(a);                                                                                    \
        r |= b;                                                                                                        \
        return r;                                                                                                      \
    }                                                                                                                  \
    inline Flags<Enum, uint32_t> operator&(Enum a, Enum b)                                                             \
    {                                                                                                                  \
        Flags<Enum, uint32_t> r(a);                                                                                    \
        r &= b;                                                                                                        \
        return r;                                                                                                      \
    }                                                                                                                  \
    inline Flags<Enum, uint32_t> operator~(Enum a) { return ~Flags<Enum, uint32_t>(a); }

} // namespace Crowny