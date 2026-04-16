#include <catch2/catch_test_macros.hpp>
#include <array>
#include "Platform/Vulkan/VulkanUtils.h"

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
            if (output[i].baseMipLevel == 2 && output[i].levelCount == 4 &&
                output[i].baseArrayLayer == 2 && output[i].layerCount == 4)
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
            if (output[i].baseMipLevel == 0 && output[i].levelCount == 5 &&
                output[i].baseArrayLayer == 0 && output[i].layerCount == 5)
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
