#include "Platform/Vulkan/VulkanUtils.h"
#include <array>
#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

TEST_CASE("VulkanUtils::CutHorizontal", "[VulkanUtils]")
{
    VkImageSubresourceRange toCut = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 10 };
    VkImageSubresourceRange cutWith = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 2, 4 };
    VkImageSubresourceRange output[3];
    uint32_t numAreas = 0;

    SECTION("Partial overlap - middle")
    {
        VulkanUtils::CutHorizontal(toCut, cutWith, output, numAreas);
        REQUIRE(numAreas == 3);
        CHECK(output[0].baseArrayLayer == 0);
        CHECK(output[0].layerCount == 2);
        CHECK(output[1].baseArrayLayer == 6);
        CHECK(output[1].layerCount == 4);
        CHECK(output[2].baseArrayLayer == 2);
        CHECK(output[2].layerCount == 4);
    }

    SECTION("No overlap - left")
    {
        VkImageSubresourceRange noOverlap = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 20, 5 };
        VulkanUtils::CutHorizontal(toCut, noOverlap, output, numAreas);
        REQUIRE(numAreas == 1);
        CHECK(output[0].baseArrayLayer == 0);
        CHECK(output[0].layerCount == 10);
    }

    SECTION("Full containment - cutWith covers toCut")
    {
        VkImageSubresourceRange covers = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 20 };
        VulkanUtils::CutHorizontal(toCut, covers, output, numAreas);
        REQUIRE(numAreas == 1);
        CHECK(output[0].baseArrayLayer == 0);
        CHECK(output[0].layerCount == 10);
    }

    SECTION("Overlap - left edge")
    {
        VkImageSubresourceRange leftEdge = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 5 };
        VulkanUtils::CutHorizontal(toCut, leftEdge, output, numAreas);
        REQUIRE(numAreas == 2);
        CHECK(output[0].baseArrayLayer == 5);
        CHECK(output[0].layerCount == 5);
        CHECK(output[1].baseArrayLayer == 0);
        CHECK(output[1].layerCount == 5);
    }
}

TEST_CASE("VulkanUtils::CutVertical", "[VulkanUtils]")
{
    VkImageSubresourceRange toCut = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 10, 0, 1 };
    VkImageSubresourceRange cutWith = { VK_IMAGE_ASPECT_COLOR_BIT, 2, 4, 0, 1 };
    VkImageSubresourceRange output[3];
    uint32_t numAreas = 0;

    SECTION("Partial overlap - middle")
    {
        VulkanUtils::CutVertical(toCut, cutWith, output, numAreas);
        REQUIRE(numAreas == 3);
        CHECK(output[0].baseMipLevel == 0);
        CHECK(output[0].levelCount == 2);
        CHECK(output[1].baseMipLevel == 6);
        CHECK(output[1].levelCount == 4);
        CHECK(output[2].baseMipLevel == 2);
        CHECK(output[2].levelCount == 4);
    }
}

TEST_CASE("VulkanUtils::CutRange", "[VulkanUtils]")
{
    VkImageSubresourceRange toCut = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 10, 0, 10 };
    std::array<VkImageSubresourceRange, 9> output;
    uint32_t numAreas = 0;

    SECTION("Center cut - now produces 5 fragments")
    {
        VkImageSubresourceRange center = { VK_IMAGE_ASPECT_COLOR_BIT, 2, 4, 2, 4 };
        VulkanUtils::CutRange(toCut, center, output, numAreas);
        REQUIRE(numAreas == 5);

        bool foundCenter = false;
        for (uint32_t i = 0; i < numAreas; ++i)
        {
            if (output[i].baseMipLevel == 2 && output[i].levelCount == 4 && output[i].baseArrayLayer == 2 && output[i].layerCount == 4)
            {
                foundCenter = true;
                break;
            }
        }
        CHECK(foundCenter);
    }

    SECTION("No overlap")
    {
        VkImageSubresourceRange noOverlap = { VK_IMAGE_ASPECT_COLOR_BIT, 20, 5, 20, 5 };
        VulkanUtils::CutRange(toCut, noOverlap, output, numAreas);
        REQUIRE(numAreas == 1);
        CHECK(output[0].baseMipLevel == 0);
        CHECK(output[0].levelCount == 10);
        CHECK(output[0].baseArrayLayer == 0);
        CHECK(output[0].layerCount == 10);
    }

    SECTION("Partial overlap - corner")
    {
        VkImageSubresourceRange corner = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 5, 0, 5 };
        VulkanUtils::CutRange(toCut, corner, output, numAreas);
        REQUIRE(numAreas == 3);

        bool foundCorner = false;
        for (uint32_t i = 0; i < numAreas; ++i)
        {
            if (output[i].baseMipLevel == 0 && output[i].levelCount == 5 && output[i].baseArrayLayer == 0 && output[i].layerCount == 5)
            {
                foundCorner = true;
                break;
            }
        }
        CHECK(foundCorner);
    }

    SECTION("Different aspects - should not cut")
    {
        VkImageSubresourceRange depthRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 10, 0, 10 };
        VulkanUtils::CutRange(toCut, depthRange, output, numAreas);
        REQUIRE(numAreas == 1);
        CHECK(output[0].aspectMask == VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

TEST_CASE("VulkanUtils maps normalized viewports to framebuffer rectangles", "[VulkanUtils]")
{
    SECTION("Regular viewport")
    {
        const Rect2I rect = VulkanUtils::GetViewportRect(Rect2F(0.25f, 0.5f, 0.5f, 0.25f), 800, 600);
        CHECK(rect == Rect2I(200, 300, 400, 150));
    }

    SECTION("Viewport is clamped to the framebuffer")
    {
        const Rect2I rect = VulkanUtils::GetViewportRect(Rect2F(-0.25f, 0.5f, 1.5f, 1.0f), 800, 600);
        CHECK(rect == Rect2I(0, 300, 800, 300));
    }
}

TEST_CASE("VulkanUtils derives pipeline stages from access masks", "[VulkanUtils]")
{
    CHECK(VulkanUtils::GetPipelineStageFlags(0) == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    CHECK(VulkanUtils::GetPipelineStageFlags(VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT) == VK_PIPELINE_STAGE_TRANSFER_BIT);
    CHECK(VulkanUtils::GetPipelineStageFlags(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT) == VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    const VkPipelineStageFlags shaderStages = VulkanUtils::GetPipelineStageFlags(VK_ACCESS_SHADER_READ_BIT);
    CHECK((shaderStages & VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT) != 0);
    CHECK((shaderStages & VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) != 0);
}

TEST_CASE("VulkanUtils maps BGRA and block-compressed texture formats", "[VulkanUtils]")
{
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::BGRA8, false) == VK_FORMAT_B8G8R8A8_UNORM);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::BGRA8, true) == VK_FORMAT_B8G8R8A8_SRGB);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::BC1, false) == VK_FORMAT_BC1_RGB_UNORM_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::BC1a, true) == VK_FORMAT_BC1_RGBA_SRGB_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::BC2, false) == VK_FORMAT_BC2_UNORM_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::BC3, true) == VK_FORMAT_BC3_SRGB_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::BC4, true) == VK_FORMAT_BC4_UNORM_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::BC5, false) == VK_FORMAT_BC5_UNORM_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::BC6H, false) == VK_FORMAT_BC6H_UFLOAT_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::BC7, true) == VK_FORMAT_BC7_SRGB_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::ETC2_RGB, true) == VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::ETC2_RGBA, false) == VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::ETC2_R11, false) == VK_FORMAT_EAC_R11_UNORM_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::ETC2_RG11, false) == VK_FORMAT_EAC_R11G11_UNORM_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::ASTC4x4, true) == VK_FORMAT_ASTC_4x4_SRGB_BLOCK);
    CHECK(VulkanUtils::GetTextureFormat(TextureFormat::NONE, false) == VK_FORMAT_UNDEFINED);
}
