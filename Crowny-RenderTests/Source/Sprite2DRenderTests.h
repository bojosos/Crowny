#pragma once

#include "RenderTestImage.h"

namespace Crowny::RenderTests
{
    bool RenderPersistentSprites(Image& image, String& error);
    bool RenderMixed2DOrder(Image& image, String& error);
    bool RenderPersistentText(Image& image, String& error);
    bool RenderIntegerClearDraw(Image& image, String& error);
    int RunSprite2DBenchmark(const Path& artifacts, bool smoke);
} // namespace Crowny::RenderTests
