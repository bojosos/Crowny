#include "cwpch.h"

#include "Crowny/Renderer/ShaderVariation.h"

namespace Crowny
{
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
        return (it != m_Parameters.end()) ? it->second.I : 0;
    }

    float ShaderVariation::GetFloat(const String& name) const
    {
        auto it = m_Parameters.find(name);
        return (it != m_Parameters.end()) ? it->second.F : 0.0f;
    }

    bool ShaderVariation::GetBool(const String& name) const
    {
        auto it = m_Parameters.find(name);
        return (it != m_Parameters.end()) ? (it->second.I != 0) : false;
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
            if (value.I != findIter->second.I)
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
                if (value.I != findIter->second.I)
                    return false;
            }
        }

        return true;
    }

    // --- ShaderDefines ---

    void ShaderDefines::Set(const String& name, int value) { m_Defines[name] = std::to_string(value); }

    void ShaderDefines::Set(const String& name, float value) { m_Defines[name] = std::to_string(value); }

    void ShaderDefines::Set(const String& name, const String& value) { m_Defines[name] = value; }

    const UnorderedMap<String, String>& ShaderDefines::Get() const { return m_Defines; }

} // namespace Crowny
