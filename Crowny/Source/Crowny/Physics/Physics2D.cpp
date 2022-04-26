#include "cwpch.h"

#include "Crowny/Physics/Physics2D.h"

namespace Crowny
{

    Physics2D::Physics2D()
    {
        m_Settings = CreateRef<Physics2DSettings>();
        for (uint32_t i = 0; i < m_MaskBits.size(); i++)
            m_MaskBits[i] = 0xFFFFFFFF;
    }

} // namespace Crowny