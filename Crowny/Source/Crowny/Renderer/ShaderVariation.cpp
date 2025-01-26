#include "cwpch.h"

#include "Crowny/Renderer/ShaderVariation.h"

namespace Crowny
{
    ShaderVariation::ShaderVariation(const Vector<ShaderVariation::Specifier>& parameters) : m_Parameters(parameters) {}

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

} // namespace Crowny