#include "cwpch.h"

#include "Crowny/ImGui/ImGuiVulkanTexture.h"

#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/RenderAPI/SamplerState.h"
#include "Crowny/RenderAPI/Texture.h"

#include "Platform/Vulkan/VulkanCommandBuffer.h"
#include "Platform/Vulkan/VulkanRenderAPI.h"
#include "Platform/Vulkan/VulkanSamplerState.h"
#include "Platform/Vulkan/VulkanTexture.h"
#include "Platform/OpenGL/OpenGLTexture.h"

#include <backends/imgui_impl_vulkan.h>

namespace Crowny
{
    namespace
    {
        struct TextureBinding
        {
            Ref<Texture> TextureRef;
            VulkanImage* Image = nullptr;
            VkImageView ImageView = VK_NULL_HANDLE;
            VkSampler Sampler = VK_NULL_HANDLE;
            VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
        };

        struct RetiredDescriptorSet
        {
            Ref<Texture> TextureRef;
            VulkanImage* Image = nullptr;
            VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
            bool PassedRenderBoundary = false;
        };

        UnorderedMap<Texture*, TextureBinding> s_Bindings;
        Vector<RetiredDescriptorSet> s_RetiredDescriptorSets;
        Vector<Ref<Texture>> s_RetiredOpenGLTextures;

        template <typename T> ImTextureID ToTextureID(T descriptorSet)
        {
            if constexpr (std::is_pointer_v<T>)
                return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(descriptorSet));
            else
                return static_cast<ImTextureID>(descriptorSet);
        }
    } // namespace

    ImTextureID ImGuiVulkanTexture::Get(const Ref<Texture>& texture)
    {
        if (texture == nullptr)
            return ImTextureID_Invalid;

        if (RenderAPI::GetAPI() == RenderAPI::API::OpenGL)
            return ToTextureID(static_cast<OpenGLTexture*>(texture.get())->GetRendererID());

        VulkanTexture* vulkanTexture = static_cast<VulkanTexture*>(texture.get());
        VulkanImage* image = vulkanTexture->GetImage();
        CW_ENGINE_ASSERT(image != nullptr);

        const Ref<SamplerState>& samplerState = SamplerState::GetDefault();
        VulkanSamplerState* vulkanSampler = static_cast<VulkanSamplerState*>(samplerState.get());
        const VkSampler sampler = vulkanSampler->GetSampler()->GetHandle();
        const VkImageView imageView = image->GetView(false);

        auto binding = s_Bindings.find(texture.get());
        if (binding != s_Bindings.end())
        {
            const TextureBinding& entry = binding->second;
            if (entry.Image == image && entry.ImageView == imageView && entry.Sampler == sampler)
                return ToTextureID(entry.DescriptorSet);

            // The current draw list will use the replacement descriptor. The old
            // binding only needs to survive already-submitted GPU work.
            s_RetiredDescriptorSets.push_back({ entry.TextureRef, nullptr, entry.DescriptorSet, true });
            s_Bindings.erase(binding);
        }

        const VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (descriptorSet == VK_NULL_HANDLE)
            return ImTextureID_Invalid;

        s_Bindings.emplace(texture.get(), TextureBinding{ texture, image, imageView, sampler, descriptorSet });
        return ToTextureID(descriptorSet);
    }

    void ImGuiVulkanTexture::Release(const Ref<Texture>& texture)
    {
        if (texture == nullptr)
            return;
        if (RenderAPI::GetAPI() == RenderAPI::API::OpenGL)
        {
            // ImGui stores a raw GLuint in its draw list. Keep the Texture alive
            // until all draw lists for this frame have been submitted.
            s_RetiredOpenGLTextures.push_back(texture);
            return;
        }
        const auto binding = s_Bindings.find(texture.get());
        if (binding == s_Bindings.end())
            return;
        s_RetiredDescriptorSets.push_back(
          { binding->second.TextureRef, binding->second.Image, binding->second.DescriptorSet, false });
        s_Bindings.erase(binding);
    }

    void ImGuiVulkanTexture::PrepareForRender(VulkanCmdBuffer* commandBuffer)
    {
        CW_ENGINE_ASSERT(commandBuffer != nullptr);
        const bool requiresGpuDrain = std::any_of(s_RetiredDescriptorSets.begin(), s_RetiredDescriptorSets.end(),
                                                  [](const RetiredDescriptorSet& retired) { return retired.PassedRenderBoundary; });
        if (requiresGpuDrain)
            gVulkanRenderAPI().GetPresentDevice()->WaitIdle();

        for (auto retired = s_RetiredDescriptorSets.begin(); retired != s_RetiredDescriptorSets.end();)
        {
            if (retired->PassedRenderBoundary)
            {
                ImGui_ImplVulkan_RemoveTexture(retired->DescriptorSet);
                retired = s_RetiredDescriptorSets.erase(retired);
            }
            else
            {
                if (retired->Image != nullptr)
                {
                    const VkImageSubresourceRange range = retired->Image->GetRange(TextureSurface::COMPLETE);
                    commandBuffer->RegisterImageShader(retired->Image, range, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                       VulkanAccessFlagBits::Read, VK_SHADER_STAGE_FRAGMENT_BIT);
                }
                retired->PassedRenderBoundary = true;
                ++retired;
            }
        }
        for (const auto& [texture, binding] : s_Bindings)
        {
            const VkImageSubresourceRange range = binding.Image->GetRange(TextureSurface::COMPLETE);
            commandBuffer->RegisterImageShader(binding.Image, range, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VulkanAccessFlagBits::Read,
                                               VK_SHADER_STAGE_FRAGMENT_BIT);
        }
    }

    void ImGuiVulkanTexture::FinishOpenGLFrame()
    {
        if (RenderAPI::GetAPI() == RenderAPI::API::OpenGL)
            s_RetiredOpenGLTextures.clear();
    }

    void ImGuiVulkanTexture::Clear()
    {
        for (const auto& [texture, binding] : s_Bindings)
            ImGui_ImplVulkan_RemoveTexture(binding.DescriptorSet);
        for (const RetiredDescriptorSet& retired : s_RetiredDescriptorSets)
            ImGui_ImplVulkan_RemoveTexture(retired.DescriptorSet);

        s_Bindings.clear();
        s_RetiredDescriptorSets.clear();
        s_RetiredOpenGLTextures.clear();
    }
} // namespace Crowny
