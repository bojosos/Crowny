#include "cwpch.h"

#include "Crowny/Renderer/ShaderVariation.h"

#include "Crowny/Common/Hash.h"

#include <charconv>
#include <limits>

namespace Crowny
{
    namespace
    {
        void AppendKeyPart(String& output, StringView value)
        {
            output += std::to_string(value.size());
            output.push_back(':');
            output.append(value);
        }

        uint32_t FloatBits(float value)
        {
            uint32_t bits;
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        }

        String FloatToString(float value)
        {
            char buffer[64];
            const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general,
                                              std::numeric_limits<float>::max_digits10);
            return result.ec == std::errc() ? String(buffer, result.ptr) : String("0");
        }
    } // namespace

    const ShaderVariation ShaderVariation::EMPTY;

    ShaderVariation::ShaderVariation(const Vector<Specifier>& specifiers)
    {
        for (const auto& s : specifiers)
            m_Parameters[s.Name] = s;
    }

    ShaderVariation::ShaderVariation(std::initializer_list<Specifier> specifiers)
    {
        for (const auto& s : specifiers)
            m_Parameters[s.Name] = s;
    }

    void ShaderVariation::Set(const String& name, int32_t value) { m_Parameters[name] = Specifier(name, value); }

    void ShaderVariation::Set(const String& name, float value) { m_Parameters[name] = Specifier(name, value); }

    void ShaderVariation::Set(const String& name, bool value) { m_Parameters[name] = Specifier(name, value); }

    int32_t ShaderVariation::GetInt(const String& name) const
    {
        auto it = m_Parameters.find(name);
        return (it != m_Parameters.end() && it->second.Type == Specifier::Int) ? it->second.I : 0;
    }

    float ShaderVariation::GetFloat(const String& name) const
    {
        auto it = m_Parameters.find(name);
        return (it != m_Parameters.end() && it->second.Type == Specifier::Float) ? it->second.F : 0.0f;
    }

    bool ShaderVariation::GetBool(const String& name) const
    {
        auto it = m_Parameters.find(name);
        return (it != m_Parameters.end() && it->second.Type == Specifier::Bool) ? (it->second.I != 0) : false;
    }

    bool ShaderVariation::Has(const String& name) const { return m_Parameters.find(name) != m_Parameters.end(); }

    ShaderDefines ShaderVariation::GetDefines() const
    {
        ShaderDefines result;
        for (const auto& [name, value] : m_Parameters)
        {
            if (value.Type == Specifier::Bool || value.Type == Specifier::Int)
                result.Set(name, value.I);
            else if (value.Type == Specifier::Float)
                result.Set(name, value.F);
        }

        return result;
    }

    bool ShaderVariation::Matches(const ShaderVariation& other, bool exact) const
    {
        // All from other are in this->m_Parameters.
        for (const auto& [name, value] : other.m_Parameters)
        {
            const auto findIter = m_Parameters.find(name);
            if (findIter == m_Parameters.end())
                return false;
            if (value.Type != findIter->second.Type)
                return false;
            if (value.Type == Specifier::Float ? FloatBits(value.F) != FloatBits(findIter->second.F) : value.I != findIter->second.I)
                return false;
        }

        // All from this->m_Parameters are in other.
        if (exact)
        {
            for (const auto& [name, value] : m_Parameters)
            {
                const auto findIter = other.m_Parameters.find(name);
                if (findIter == other.m_Parameters.end())
                    return false;
                if (value.Type != findIter->second.Type)
                    return false;
                if (value.Type == Specifier::Float ? FloatBits(value.F) != FloatBits(findIter->second.F) : value.I != findIter->second.I)
                    return false;
            }
        }

        return true;
    }

    String ShaderVariation::GetCanonicalKey() const
    {
        Vector<const Specifier*> sorted;
        sorted.reserve(m_Parameters.size());
        for (const auto& [_, value] : m_Parameters)
            sorted.push_back(&value);
        std::sort(sorted.begin(), sorted.end(), [](const Specifier* lhs, const Specifier* rhs) { return lhs->Name < rhs->Name; });

        String result;
        for (const Specifier* value : sorted)
        {
            AppendKeyPart(result, value->Name);
            result.push_back('=');
            result += std::to_string(static_cast<uint32_t>(value->Type));
            result.push_back(':');
            result += value->Type == Specifier::Float ? std::to_string(FloatBits(value->F)) : std::to_string(value->I);
            result.push_back(';');
        }
        return result;
    }

    uint64_t ShaderVariation::GetHash() const { return Hashing::CityHash64(GetCanonicalKey()); }

    // --- ShaderDefines ---

    void ShaderDefines::Set(const String& name, int value) { m_Defines[name] = std::to_string(value); }

    void ShaderDefines::Set(const String& name, float value) { m_Defines[name] = FloatToString(value); }

    void ShaderDefines::Set(const String& name, const String& value) { m_Defines[name] = value; }

    void ShaderDefines::Remove(const String& name) { m_Defines.erase(name); }

    bool ShaderDefines::Has(const String& name) const { return m_Defines.find(name) != m_Defines.end(); }

    const UnorderedMap<String, String>& ShaderDefines::Get() const { return m_Defines; }

    String ShaderDefines::GetCanonicalKey() const
    {
        Vector<Pair<StringView, StringView>> sorted;
        sorted.reserve(m_Defines.size());
        for (const auto& [name, value] : m_Defines)
            sorted.emplace_back(name, value);
        std::sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        String result;
        for (const auto& [name, value] : sorted)
        {
            AppendKeyPart(result, name);
            result.push_back('=');
            AppendKeyPart(result, value);
            result.push_back(';');
        }
        return result;
    }

    uint64_t ShaderDefines::GetHash() const { return Hashing::CityHash64(GetCanonicalKey()); }

} // namespace Crowny
