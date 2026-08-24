#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/ImGui/ImGuiVulkanLayer.h"
#include "Crowny/ImGui/ImGuiVulkanTexture.h"
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
        const VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                                                    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                                                    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                                                    { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCreateInfo.maxSets = 1000;
        poolCreateInfo.poolSizeCount = (uint32_t)std::size(pool_sizes);
        poolCreateInfo.pPoolSizes = pool_sizes;

        const VkResult result =
          vkCreateDescriptorPool(gVulkanRenderAPI().GetPresentDevice()->GetLogicalDevice(), &poolCreateInfo, gVulkanAllocator, &m_ImguiPool);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        Application& app = (*Application::TryGet());
        GLFWwindow* const window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());
        VulkanRenderWindow* const renderWindow = static_cast<VulkanRenderWindow*>(app.GetRenderWindow().get());
        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.ApiVersion = VK_API_VERSION_1_1;
        init_info.Instance = gVulkanRenderAPI().GetInstance();
        init_info.PhysicalDevice = gVulkanRenderAPI().GetPresentDevice()->GetPhysicalDevice();
        init_info.Device = gVulkanRenderAPI().GetPresentDevice()->GetLogicalDevice();
        const uint32_t numQueues = gVulkanRenderAPI().GetPresentDevice()->GetNumQueues(GRAPHICS_QUEUE);
        init_info.Queue = gVulkanRenderAPI().GetPresentDevice()->GetQueue(GRAPHICS_QUEUE, numQueues - 1)->GetHandle();
        init_info.DescriptorPool = m_ImguiPool;
        init_info.MinImageCount = renderWindow->GetSwapChain()->GetColorSurfacesCount();
        init_info.ImageCount = renderWindow->GetSwapChain()->GetColorSurfacesCount();
        init_info.Allocator = gVulkanAllocator;
        init_info.QueueFamily = gVulkanRenderAPI().GetPresentDevice()->GetQueueFamily(GRAPHICS_QUEUE);
        init_info.PipelineCache = gVulkanRenderAPI().GetPresentDevice()->GetPipelineCache();
        // init_info.CheckVkResultFn = [](VkResult result) { CW_ENGINE_ASSERT(result == VK_SUCCESS); };

        VulkanRenderPassDesc passDesc;
        passDesc.Samples = 1;
        passDesc.Offscreen = false;
        passDesc.Color[0].Format = renderWindow->GetColorFormat();
        passDesc.Color[0].Enabled = true;
        passDesc.Depth.Enabled = false;

        m_RenderPass = VulkanRenderPasses::Get().GetRenderPass(passDesc);
        init_info.PipelineInfoMain.RenderPass = m_RenderPass->GetVkRenderPass(RT_NONE, RT_NONE, CLEAR_COLOR0);
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.PipelineInfoForViewports.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        ImGui_ImplVulkan_Init(&init_info);
    }

    void ImGuiVulkanLayer::OnDetach()
    {
        gVulkanRenderAPI().GetPresentDevice()->WaitIdle();
        ImGuiVulkanTexture::Clear();
        ImGui_ImplVulkan_Shutdown();
        ImGuiLayer::OnDetach();
        vkDestroyDescriptorPool(gVulkanRenderAPI().GetPresentDevice()->GetLogicalDevice(), m_ImguiPool, gVulkanAllocator);
    }

    void ImGuiVulkanLayer::Begin()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGuiLayer::Begin();
        RenderAPI::TryGet()->SetRenderTarget(Application::TryGet()->GetRenderWindow());
    }

    void ImGuiVulkanLayer::End()
    {
        ImGuiLayer::End();

        VulkanCmdBuffer* vkCmdBuffer = gVulkanRenderAPI().GetMainCommandBuffer()->GetInternal();
        ImGuiVulkanTexture::PrepareForRender(vkCmdBuffer);
        gVulkanRenderAPI().SubmitCommandBuffer(nullptr);

        vkCmdBuffer = gVulkanRenderAPI().GetMainCommandBuffer()->GetInternal();
        RenderAPI::TryGet()->SetRenderTarget(Application::TryGet()->GetRenderWindow());
        RenderAPI::TryGet()->SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        gVulkanRenderAPI().ClearRenderTarget(FBT_COLOR);
        vkCmdBuffer->BeginRenderPass();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmdBuffer->GetHandle());

        const ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

} // namespace Crowny
