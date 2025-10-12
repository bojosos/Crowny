#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/ImGui/ImGuiVulkanLayer.h"
#include "Crowny/Window/Window.h"

#include "Crowny/RenderAPI/CommandBuffer.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanQueue.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanRenderWindow.h"
#include "Platform/Vulkan/VulkanSwapChain.h"

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <imgui.h>

namespace Crowny
{

    ImGuiVulkanLayer::ImGuiVulkanLayer() : ImGuiLayer() {}

    void ImGuiVulkanLayer::OnAttach()
    {
        ImGuiLayer::OnAttach();
        Application& app = Application::Get();
        GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());
        ImGui_ImplGlfw_InitForVulkan(window, true);
        m_Backend.Init();
    }

    void ImGuiVulkanLayer::OnDetach()
    {
    }

    void ImGuiVulkanLayer::Begin()
    {
        ImGui_ImplGlfw_NewFrame();
        ImGuiLayer::Begin();
    }

    void ImGuiVulkanLayer::End()
    {
        ImGuiLayer::End();
        // m_Backend.RenderDrawData(ImGui::GetDrawData());
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

} // namespace Crowny
