#pragma once

#include "Crowny/ImGui/ImGuiLayer.h"
#include "Crowny/ImGui/ImGuiBackend.h"

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

        virtual ImGuiBackend* GetBackend() override { return &m_Backend; }

        virtual void Begin() override;
        virtual void End() override;

    private:
        ImGuiBackend m_Backend;
    };
} // namespace Crowny