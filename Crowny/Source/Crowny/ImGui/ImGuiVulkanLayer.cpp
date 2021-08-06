#include "cwpch.h"

#include "Crowny/Application/Application.h"
#include "Crowny/ImGui/ImGuiVulkanLayer.h"
#include "Crowny/Input/Input.h"

#include "Crowny/RenderAPI/CommandBuffer.h"
#include "Crowny/RenderAPI/RenderTarget.h"
#include "Crowny/RenderAPI/RenderTexture.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanRenderPass.h"
#include "Platform/Vulkan/VulkanRenderWindow.h"
#include "Platform/Vulkan/VulkanTexture.h"

#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>
#include <vulkan/vulkan.hpp>

namespace Crowny
{

    extern Ref<RenderTarget> renderTarget;

    ImGuiVulkanLayer::ImGuiVulkanLayer() : ImGuiLayer() {}

    void ImGuiVulkanLayer::OnAttach()
    {
        ImGuiLayer::OnAttach();
        VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                                              { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                                              { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                                              { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                                              { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                                              { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                                              { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                                              { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                                              { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                                              { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                                              { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCreateInfo.maxSets = 1000;
        poolCreateInfo.poolSizeCount = std::size(pool_sizes);
        poolCreateInfo.pPoolSizes = pool_sizes;

        VkDescriptorPool imguiPool;
        VkResult result = vkCreateDescriptorPool(gVulkanRenderAPI().GetPresentDevice()->GetLogicalDevice(),
                                                 &poolCreateInfo, nullptr, &imguiPool);
        CW_ENGINE_ASSERT(result == VK_SUCCESS);

        Application& app = Application::Get();
        GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());
        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.Instance = gVulkanRenderAPI().GetInstance();
        init_info.PhysicalDevice = gVulkanRenderAPI().GetPresentDevice()->GetPhysicalDevice();
        init_info.Device = gVulkanRenderAPI().GetPresentDevice()->GetLogicalDevice();
        uint32_t numQueues = gVulkanRenderAPI().GetPresentDevice()->GetNumQueues(GRAPHICS_QUEUE);
        init_info.Queue = gVulkanRenderAPI().GetPresentDevice()->GetQueue(GRAPHICS_QUEUE, numQueues - 1)->GetHandle();
        init_info.DescriptorPool = imguiPool;
        init_info.MinImageCount = static_cast<VulkanRenderWindow*>(Application::Get().GetRenderWindow().get())
                                    ->GetSwapChain()
                                    ->GetColorSurfacesCount();
        init_info.ImageCount = static_cast<VulkanRenderWindow*>(Application::Get().GetRenderWindow().get())
                                 ->GetSwapChain()
                                 ->GetColorSurfacesCount();
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = gVulkanAllocator;
        init_info.QueueFamily = gVulkanRenderAPI().GetPresentDevice()->GetQueueFamily(GRAPHICS_QUEUE);

        VulkanRenderPassDesc passDesc;
        passDesc.Samples = 1;
        passDesc.Offscreen = false;
        passDesc.Color[0].Format = VK_FORMAT_B8G8R8A8_UNORM;
        passDesc.Color[0].Enabled = true;
        passDesc.Depth.Enabled = true;
        passDesc.Depth.Format = VK_FORMAT_D32_SFLOAT_S8_UINT;

        VulkanRenderPass* renderPass = VulkanRenderPasses::Get().GetRenderPass(passDesc);
        ImGui_ImplVulkan_Init(
          &init_info, renderPass->GetVkRenderPass((RenderSurfaceMaskBits)0, (RenderSurfaceMaskBits)0, (ClearMask)0));

        Ref<VulkanCommandBuffer> cmdBuffer =
          std::static_pointer_cast<VulkanCommandBuffer>(CommandBuffer::Create(GRAPHICS_QUEUE));
        ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer->GetInternal()->GetHandle());
        gVulkanRenderAPI().SubmitCommandBuffer(cmdBuffer);
        cmdBuffer->GetInternal()->CheckFenceStatus(true);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void ImGuiVulkanLayer::OnDetach()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGuiLayer::OnDetach();
    }

    void ImGuiVulkanLayer::Begin()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGuiLayer::Begin();
    }

    void ImGuiVulkanLayer::End()
    {
        ImGuiLayer::End();

        VulkanCmdBuffer* vkCmdBuffer = gVulkanRenderAPI().GetMainCommandBuffer()->GetInternal();

        RenderAPI::Get().SetRenderTarget(Application::Get().GetRenderWindow());

        RenderTexture* rt = static_cast<RenderTexture*>(renderTarget.get());
        Ref<Texture> texture = rt->GetColorTexture(0);
        VulkanTexture* vkTexture = static_cast<VulkanTexture*>(texture.get());
        VulkanImage* image = vkTexture->GetImage();

        VkImageSubresourceRange range = image->GetRange(TextureSurface::COMPLETE);
        vkCmdBuffer->RegisterImageShader(image, range, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                         VulkanAccessFlagBits::Read, VK_SHADER_STAGE_FRAGMENT_BIT);
        gVulkanRenderAPI().GetMainCommandBuffer()->GetInternal()->BeginRenderPass();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmdBuffer->GetHandle());
        gVulkanRenderAPI().GetMainCommandBuffer()->GetInternal()->EndRenderPass();

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

} // namespace Crowny
