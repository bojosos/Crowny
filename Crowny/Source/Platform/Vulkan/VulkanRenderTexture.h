#pragma once

#include "Crowny/RenderAPI/RenderTexture.h"

#include "Platform/Vulkan/VulkanFramebuffer.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{

    class VulkanRenderTexture : public RenderTexture
    {
    public:
        VulkanRenderTexture(const RenderTextureProperties& props);
        ~VulkanRenderTexture();

        VulkanFramebuffer* GetFramebuffer() const { return m_Framebuffer; }

        virtual void Resize(uint32_t width, uint32_t height) override{};
        virtual const RenderTextureProperties& GetProperties() const override { return m_Props; }
        virtual void SwapBuffers(uint32_t syncMask = 0xFFFFFFFF) override{};

    private:
        VulkanFramebuffer* m_Framebuffer;
    };

} // namespace Crowny