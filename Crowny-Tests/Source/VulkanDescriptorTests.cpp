#include <catch2/catch_test_macros.hpp>

#include "Platform/Vulkan/VulkanDescriptorPool.h"

using namespace Crowny;

TEST_CASE("Descriptor arrays update changed ranges against each cached set", "[Renderer][Vulkan][Bindless]")
{
    Vector<VkDescriptorImageInfo> images(8);
    for (auto& image : images)
        image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet binding{};
    binding.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    binding.dstBinding = 1;
    binding.descriptorCount = static_cast<uint32_t>(images.size());
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.pImageInfo = images.data();
    Vector<VkDescriptorImageInfo> firstSet;
    Vector<VkDescriptorImageInfo> secondSet;
    Vector<VkWriteDescriptorSet> writes;
    VulkanDescriptorSet::AppendImageWrites(binding, firstSet, writes);
    REQUIRE(writes.size() == 1);
    CHECK(writes[0].descriptorCount == 8);
    writes.clear();
    VulkanDescriptorSet::AppendImageWrites(binding, secondSet, writes);
    writes.clear();
    VulkanDescriptorSet::AppendImageWrites(binding, firstSet, writes);
    CHECK(writes.empty());

    images[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    images[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    images[6].imageView = reinterpret_cast<VkImageView>(uintptr_t{ 1 });
    VulkanDescriptorSet::AppendImageWrites(binding, firstSet, writes);
    REQUIRE(writes.size() == 2);
    CHECK(writes[0].dstBinding == 1);
    CHECK(writes[0].dstArrayElement == 2);
    CHECK(writes[0].descriptorCount == 2);
    CHECK(writes[0].pImageInfo == images.data() + 2);
    CHECK(writes[1].dstArrayElement == 6);

    // A dormant pass/set must receive changes it missed, even when the most recent set is current.
    images[0].sampler = reinterpret_cast<VkSampler>(uintptr_t{ 1 });
    writes.clear();
    VulkanDescriptorSet::AppendImageWrites(binding, secondSet, writes);
    REQUIRE(writes.size() == 3);
    CHECK(writes[0].dstArrayElement == 0);
    CHECK(writes[1].dstArrayElement == 2);
    CHECK(writes[2].dstArrayElement == 6);
    images[6] = images[7];
    writes.clear();
    VulkanDescriptorSet::AppendImageWrites(binding, secondSet, writes);
    REQUIRE(writes.size() == 1);
    CHECK(writes[0].dstArrayElement == 6);
    CHECK(writes[0].pImageInfo->imageView == VK_NULL_HANDLE);
}

TEST_CASE("Vulkan bindless capacity respects sampler and stage budgets", "[Renderer][Vulkan][Bindless]")
{
    VkPhysicalDeviceLimits limits{};
    limits.maxDescriptorSetSampledImages = 10000;
    limits.maxPerStageDescriptorSampledImages = 1024;
    limits.maxDescriptorSetSamplers = 500;
    limits.maxPerStageDescriptorSamplers = 512;
    limits.maxPerStageResources = 2048;
    CHECK(VulkanUtils::GetBindlessTextureCapacity(limits) == 212);
    limits.maxPerStageResources = 320;
    CHECK(VulkanUtils::GetBindlessTextureCapacity(limits) == 32);
    limits.maxPerStageDescriptorSamplers = 16;
    CHECK(VulkanUtils::GetBindlessTextureCapacity(limits) == 1);

    VkPhysicalDeviceDescriptorIndexingProperties indexing{};
    indexing.maxDescriptorSetUpdateAfterBindSampledImages = 20000;
    indexing.maxPerStageDescriptorUpdateAfterBindSampledImages = 20000;
    indexing.maxDescriptorSetUpdateAfterBindSamplers = 20000;
    indexing.maxPerStageDescriptorUpdateAfterBindSamplers = 8000;
    indexing.maxPerStageUpdateAfterBindResources = 10000;
    CHECK(VulkanUtils::GetBindlessTextureCapacity(limits, &indexing) == 7712);
}
