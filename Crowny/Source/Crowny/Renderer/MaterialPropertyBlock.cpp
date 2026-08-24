#include "cwpch.h"

#include "Crowny/Renderer/MaterialPropertyBlock.h"

namespace Crowny
{
    bool MaterialPropertyBlock::Remove(MaterialPropertyID property)
    {
        if (m_Values.erase(property.Value) == 0)
            return false;
        m_Revision++;
        return true;
    }

    void MaterialPropertyBlock::Clear()
    {
        if (m_Values.empty())
            return;
        m_Values.clear();
        m_Revision++;
    }

} // namespace Crowny
