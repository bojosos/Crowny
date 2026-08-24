#pragma once

#ifdef CW_WITH_NODES

#include "Crowny/Common/StringID.h"

namespace Crowny
{
    void RenderNodePalette(String& searchString, bool& grabFocus, const std::function<void(StringID)>& createNode);
}

#endif // CW_WITH_NODES
