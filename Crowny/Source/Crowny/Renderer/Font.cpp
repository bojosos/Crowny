#include "cwpch.h"

#include "Crowny/Renderer/Font.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Utils/PixelUtils.h"

#include "MSDFData.h"

#include <FontGeometry.h>
#include <GlyphGeometry.h>
#include <msdf-atlas-gen.h>

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull
#define fontScale 1.0f

namespace Crowny
{
    Font::Font(MSDFData* msdfData, const Ref<Texture>& atlasTexture)
      : m_MSDFData(msdfData), m_AtlasTexture(atlasTexture)
    {
    }

    Font::~Font() { delete m_MSDFData; }

    Ref<Font> Font::GetDefault()
    {
        // TODO: Import or something better here. Also consider the lifetime of this font.
        static Ref<Font> DefaultFont;
        // if (!DefaultFont)
        //     DefaultFont = CreateRef<Font>("Resources/Fonts/Roboto/roboto-thin.ttf");
        return DefaultFont;
    }

} // namespace Crowny