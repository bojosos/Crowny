#pragma once

#include "Crowny/Common/StdHeaders.h"

namespace Crowny::RenderTests
{
    int RunProceduralMaterialTest(const Path& package, const Path& referencePackage, const Path& artifacts);
    int CaptureProceduralPlane(const Path& package, const Path& artifacts);
} // namespace Crowny::RenderTests
