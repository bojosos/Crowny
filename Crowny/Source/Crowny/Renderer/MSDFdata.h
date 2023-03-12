#pragma once

#include "Crowny/Common/StdHeaders.h"

#undef INFINTE
#include "msdf-atlas-gen.h"

namespace Crowny
{
    struct MSDFData
    {
        Vector<msdf_atlas::GlyphGeometry> Glyphs;
        msdf_atlas::FontGeometry FontGeometry;
    };
} // namespace Crowny