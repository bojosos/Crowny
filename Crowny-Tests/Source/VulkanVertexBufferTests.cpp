#include "Platform/Vulkan/VulkanVertexBuffer.h"
#include <catch2/catch_test_macros.hpp>

using namespace Crowny;

TEST_CASE("Vulkan vertex layouts own declarations and supply missing vertex color", "[Renderer][Vulkan][VertexLayout]")
{
    const Ref<BufferLayout> meshLayout = CreateRef<BufferLayout>();
    meshLayout->AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Position));

    BufferElement shaderPosition(ShaderDataType::Float3, VertexAttribute::Position);
    shaderPosition.Location = 0;
    BufferElement shaderColor(ShaderDataType::Float4, VertexAttribute::Color);
    shaderColor.Location = 7;
    const Ref<BufferLayout> shaderLayout = CreateRef<BufferLayout>();
    shaderLayout->AddBufferElement(shaderPosition);
    shaderLayout->AddBufferElement(shaderColor);

    Ref<VulkanBufferLayout> retainedLayout;
    const VkVertexInputAttributeDescription* ownedAttributes = nullptr;
    const VkVertexInputBindingDescription* ownedBindings = nullptr;
    {
        VulkanBufferLayoutManager manager;
        retainedLayout = manager.GetBufferLayout(meshLayout, shaderLayout);
        REQUIRE(retainedLayout != nullptr);
        CHECK(retainedLayout->GetFallbackColorBinding() == 1u);

        const VkPipelineVertexInputStateCreateInfo& createInfo = retainedLayout->GetVkCreateInfo();
        REQUIRE(createInfo.vertexBindingDescriptionCount == 2u);
        REQUIRE(createInfo.vertexAttributeDescriptionCount == 2u);
        REQUIRE(createInfo.pVertexBindingDescriptions != nullptr);
        REQUIRE(createInfo.pVertexAttributeDescriptions != nullptr);
        CHECK(createInfo.pVertexBindingDescriptions[0].binding == 0u);
        CHECK(createInfo.pVertexBindingDescriptions[0].stride == 3u * sizeof(float));
        CHECK(createInfo.pVertexBindingDescriptions[1].binding == 1u);
        CHECK(createInfo.pVertexBindingDescriptions[1].stride == 0u);
        CHECK(createInfo.pVertexAttributeDescriptions[0].location == 0u);
        CHECK(createInfo.pVertexAttributeDescriptions[0].binding == 0u);
        CHECK(createInfo.pVertexAttributeDescriptions[0].format == VK_FORMAT_R32G32B32_SFLOAT);
        CHECK(createInfo.pVertexAttributeDescriptions[1].location == 7u);
        CHECK(createInfo.pVertexAttributeDescriptions[1].binding == 1u);
        CHECK(createInfo.pVertexAttributeDescriptions[1].format == VK_FORMAT_R32G32B32A32_SFLOAT);

        ownedAttributes = createInfo.pVertexAttributeDescriptions;
        ownedBindings = createInfo.pVertexBindingDescriptions;
        const Ref<VulkanBufferLayout> cached = manager.GetBufferLayout(meshLayout, shaderLayout);
        CHECK(cached == retainedLayout);
        CHECK(cached->GetVkCreateInfo().pVertexAttributeDescriptions == ownedAttributes);
        CHECK(cached->GetVkCreateInfo().pVertexBindingDescriptions == ownedBindings);

        const Ref<BufferLayout> coloredMeshLayout = CreateRef<BufferLayout>();
        coloredMeshLayout->AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Position));
        coloredMeshLayout->AddBufferElement(BufferElement(ShaderDataType::Color, VertexAttribute::Color));
        const Ref<VulkanBufferLayout> coloredLayout = manager.GetBufferLayout(coloredMeshLayout, shaderLayout);
        REQUIRE(coloredLayout != nullptr);
        CHECK(coloredLayout->GetFallbackColorBinding() == UINT32_MAX);
        CHECK(coloredLayout->GetVkCreateInfo().vertexBindingDescriptionCount == 1u);
        CHECK(coloredLayout->GetVkCreateInfo().vertexAttributeDescriptionCount == 2u);

        const Ref<BufferLayout> missingInputShader = CreateRef<BufferLayout>();
        missingInputShader->AddBufferElement(BufferElement(ShaderDataType::Float3, VertexAttribute::Normal));
        CHECK(manager.GetBufferLayout(meshLayout, missingInputShader) == nullptr);

        const Ref<BufferLayout> mismatchedMeshLayout = CreateRef<BufferLayout>();
        mismatchedMeshLayout->AddBufferElement(BufferElement(ShaderDataType::Int3, VertexAttribute::Position));
        const Ref<BufferLayout> positionShaderLayout = CreateRef<BufferLayout>();
        positionShaderLayout->AddBufferElement(shaderPosition);
        CHECK(manager.GetBufferLayout(mismatchedMeshLayout, positionShaderLayout) == nullptr);

        BufferElement overLimitPosition = shaderPosition;
        overLimitPosition.Location = 16;
        const Ref<BufferLayout> overLimitShaderLayout = CreateRef<BufferLayout>();
        overLimitShaderLayout->AddBufferElement(overLimitPosition);
        CHECK(manager.GetBufferLayout(meshLayout, overLimitShaderLayout) == nullptr);

        VkPhysicalDeviceLimits limitedDevice{};
        limitedDevice.maxVertexInputAttributes = 16;
        limitedDevice.maxVertexInputBindings = 1;
        limitedDevice.maxVertexInputAttributeOffset = 2047;
        limitedDevice.maxVertexInputBindingStride = 2048;
        VulkanBufferLayoutManager limitedManager(limitedDevice);

        BufferElement overLimitBinding(ShaderDataType::Float3, VertexAttribute::Position);
        overLimitBinding.StreamIdx = 1;
        const Ref<BufferLayout> overLimitMeshLayout = CreateRef<BufferLayout>();
        overLimitMeshLayout->AddBufferElement(overLimitBinding);
        CHECK(limitedManager.GetBufferLayout(overLimitMeshLayout, positionShaderLayout) == nullptr);
    }

    REQUIRE(retainedLayout != nullptr);
    CHECK(retainedLayout->GetVkCreateInfo().pVertexAttributeDescriptions == ownedAttributes);
    CHECK(retainedLayout->GetVkCreateInfo().pVertexBindingDescriptions == ownedBindings);
    CHECK(retainedLayout->GetVkCreateInfo().pVertexAttributeDescriptions[1].location == 7u);
}
