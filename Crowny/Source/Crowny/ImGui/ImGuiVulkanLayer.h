#pragma once

#include "Crowny/ImGui/ImGuiLayer.h"

#include <vulkan/vulkan.hpp>

namespace Crowny
{

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
        VkDescriptorPool m_ImguiPool;
    };
} // namespace Crowny