#pragma once

#include "Crowny/ImGui/ImGuiLayer.h"

typedef struct VkDescriptorPool_T* VkDescriptorPool;

namespace Crowny
{
    class VulkanRenderPass;

    class ImGuiVulkanLayer : public ImGuiLayer
    {
    public:
        ImGuiVulkanLayer();
        ~ImGuiVulkanLayer() = default;

        virtual void OnAttach() override;
        virtual void OnDetach() override;

        virtual void Begin() override;
        virtual void End() override;

    private:
        VulkanRenderPass* m_RenderPass = nullptr;
        VkDescriptorPool m_ImguiPool{};
    };
} // namespace Crowny
