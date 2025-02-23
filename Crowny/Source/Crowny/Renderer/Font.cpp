#include "cwpch.h"

#include "Crowny/Renderer/Font.h"

#include "Crowny/Common/FileSystem.h"
#include "Crowny/Common/VirtualFileSystem.h"
#include "Crowny/RenderAPI/Texture.h"
#include "Crowny/Utils/PixelUtils.h"

#include "Crowny/Renderer/MSDFdata.h"

#include <FontGeometry.h>
#include <GlyphGeometry.h>
#include <msdf-atlas-gen.h>

namespace Crowny
{
    AssetHandle<Font> Font::s_DefaultFont;

    Font::Font(MSDFData* msdfData, const Ref<Texture>& atlasTexture) : m_MSDFData(msdfData), m_AtlasTexture(atlasTexture) {}

    Font::~Font() { delete m_MSDFData; }

    AssetHandle<Font> Font::GetDefaultFont() { return s_DefaultFont; }

    void Font::SetDefaultFont(const AssetHandle<Font>& font) { s_DefaultFont = font; }

} // namespace Crowny