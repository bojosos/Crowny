#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <imgui.h>

namespace Crowny
{
    class Texture;
    class VulkanCmdBuffer;

    class ImGuiVulkanTexture
    {
    public:
        static ImTextureID Get(const Ref<Texture>& texture);
        static void Release(const Ref<Texture>& texture);
        static void PrepareForRender(VulkanCmdBuffer* commandBuffer);
        static void FinishOpenGLFrame();
        static void Clear();
    };
} // namespace Crowny
