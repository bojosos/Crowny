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

        Application& app = (*gApplication);
        GLFWwindow* const window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());
        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.Instance = gVulkanRenderAPI().GetInstance();
        init_info.PhysicalDevice = gVulkanRenderAPI().GetPresentDevice()->GetPhysicalDevice();
        init_info.Device = gVulkanRenderAPI().GetPresentDevice()->GetLogicalDevice();
        const uint32_t numQueues = gVulkanRenderAPI().GetPresentDevice()->GetNumQueues(GRAPHICS_QUEUE);
        init_info.Queue = gVulkanRenderAPI().GetPresentDevice()->GetQueue(GRAPHICS_QUEUE, numQueues - 1)->GetHandle();
        init_info.DescriptorPool = m_ImguiPool;
        init_info.MinImageCount = static_cast<VulkanRenderWindow*>(gApplication->GetRenderWindow().get())->GetSwapChain()->GetColorSurfacesCount();
        init_info.ImageCount = static_cast<VulkanRenderWindow*>(gApplication->GetRenderWindow().get())->GetSwapChain()->GetColorSurfacesCount();
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = gVulkanAllocator;
        init_info.QueueFamily = gVulkanRenderAPI().GetPresentDevice()->GetQueueFamily(GRAPHICS_QUEUE);
        init_info.PipelineCache = gVulkanRenderAPI().GetPresentDevice()->GetPipelineCache();
        // init_info.CheckVkResultFn = [](VkResult result) { CW_ENGINE_ASSERT(result == VK_SUCCESS); };
        init_info.Subpass = 0;

        VulkanRenderPassDesc passDesc;
        passDesc.Samples = 1;
        passDesc.Offscreen = false;
        passDesc.Color[0].Format = VK_FORMAT_R8G8B8A8_UNORM;
        passDesc.Color[0].Enabled = true;
        passDesc.Depth.Enabled = false;

        m_RenderPass = VulkanRenderPasses::Get().GetRenderPass(passDesc);
        ImGui_ImplVulkan_Init(&init_info, m_RenderPass->GetVkRenderPass((RenderSurfaceMaskBits)0, (RenderSurfaceMaskBits)0, CLEAR_ALL));

        const Ref<VulkanCommandBuffer> cmdBuffer = StaticRefCast<VulkanCommandBuffer>(CommandBuffer::Create(GRAPHICS_QUEUE));
        ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer->GetInternal()->GetHandle());
        gVulkanRenderAPI().SubmitCommandBuffer(cmdBuffer);
        cmdBuffer->GetInternal()->CheckFenceStatus(true);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void ImGuiVulkanLayer::OnDetach()
    {
        gVulkanRenderAPI().GetPresentDevice()->WaitIdle();
        ImGui_ImplVulkan_Shutdown();
        ImGuiLayer::OnDetach();
        vkDestroyDescriptorPool(gVulkanRenderAPI().GetPresentDevice()->GetLogicalDevice(), m_ImguiPool, gVulkanAllocator);
    }

    void ImGuiVulkanLayer::Begin()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGuiLayer::Begin();
        gRenderAPI->SetRenderTarget(gApplication->GetRenderWindow());
    }

    void ImGuiVulkanLayer::End()
    {
        ImGuiLayer::End();

        VulkanCmdBuffer* vkCmdBuffer = gVulkanRenderAPI().GetMainCommandBuffer()->GetInternal();
        ImGui_ImplVulkan_TransitionLayouts(vkCmdBuffer);
        gVulkanRenderAPI().SubmitCommandBuffer(nullptr);

        vkCmdBuffer = gVulkanRenderAPI().GetMainCommandBuffer()->GetInternal();
        gRenderAPI->SetRenderTarget(gApplication->GetRenderWindow());
        gRenderAPI->SetViewport(0.0f, 0.0f, 1.0f, 1.0f);
        gVulkanRenderAPI().ClearRenderTarget(FBT_COLOR | FBT_DEPTH);
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
