#pragma once

#include "Crowny/Common/StdHeaders.h"

struct ImDrawData;
struct ImTextureRef;
struct ImGuiViewport;

namespace Crowny
{
    class CommandBuffer;
    class IndexBuffer;
    class VertexBuffer;
    class Material;

    class ImGuiBackend
    {
    public:
        void Init();
        // void RenderDrawData(ImDrawData* drawData, const Ref<CommandBuffer>& commandBuffer=nullptr);
        ImTextureRef RegisterTexture(const Ref<Texture>& texture);

    private:
        void RenderVulkanWindow(ImGuiViewport* viewport, void*);
        // void SetupRenderState(ImDrawData* drawData, const Ref<CommandBuffer>& commandBuffer, int fbWidth, int fbHeight);
    };
}