#include "cwpch.h"

#include "Platform/Vulkan/VulkanDevice.h"
#include "Platform/Vulkan/VulkanUtils.h"

namespace Crowny
{

    VkFilter VulkanUtils::GetFilter(TextureFilter filter)
    {
        switch (filter)
        {
        case TextureFilter::LINEAR:
        case TextureFilter::ANISOTROPIC:
            return VK_FILTER_LINEAR;
        case TextureFilter::NEAREST:
        case TextureFilter::NONE:
            return VK_FILTER_NEAREST;
        }

        return VK_FILTER_LINEAR;
    }

    VkSamplerMipmapMode VulkanUtils::GetMipFilter(TextureFilter filter)
    {
        switch (filter)
        {
        case TextureFilter::LINEAR:
        case TextureFilter::ANISOTROPIC:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case TextureFilter::NEAREST:
        case TextureFilter::NONE:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        }

        return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }

    VkSamplerAddressMode VulkanUtils::GetAddressingMode(TextureWrap mode)
    {
        switch (mode)
        {
        case TextureWrap::REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case TextureWrap::MIRRORED_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case TextureWrap::CLAMP_TO_EDGE:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case TextureWrap::CLAMP_TO_BORDER:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        }
    }

    VkCompareOp VulkanUtils::GetCompareOp(CompareFunction compareFunc)
    {
        switch (compareFunc)
        {
        case CompareFunction::ALWAYS_FAIL:
            return VK_COMPARE_OP_NEVER;
        case CompareFunction::ALWAYS_PASS:
            return VK_COMPARE_OP_ALWAYS;
        case CompareFunction::LESS:
            return VK_COMPARE_OP_LESS;
        case CompareFunction::LESS_EQUAL:
            return VK_COMPARE_OP_LESS_OR_EQUAL;
        case CompareFunction::NOT_EQUAL:
            return VK_COMPARE_OP_NOT_EQUAL;
        case CompareFunction::GREATER:
            return VK_COMPARE_OP_GREATER;
        case CompareFunction::GREATER_EQUAL:
            return VK_COMPARE_OP_GREATER_OR_EQUAL;
        default:
            return VK_COMPARE_OP_ALWAYS;
        }
    }

    VkCullModeFlagBits VulkanUtils::GetCullMode(CullingMode mode)
    {
        switch (mode)
        {
        case CullingMode::CULL_NONE:
            return VK_CULL_MODE_NONE;
        case CullingMode::CULL_CLOCKWISE:
            return VK_CULL_MODE_FRONT_BIT;
        case CullingMode::CULL_COUNTERCLOCKWISE:
            return VK_CULL_MODE_BACK_BIT;
        }

        return VK_CULL_MODE_NONE;
    }

    VkBlendFactor VulkanUtils::GetBlendFactor(BlendFactor factor)
    {
        switch (factor)
        {
        case (BlendFactor::One):
            return VK_BLEND_FACTOR_ONE;
        case (BlendFactor::Zero):
            return VK_BLEND_FACTOR_ZERO;
        case (BlendFactor::DestColor):
            return VK_BLEND_FACTOR_DST_COLOR;
        case (BlendFactor::SourceColor):
            return VK_BLEND_FACTOR_SRC_COLOR;
        case (BlendFactor::InvDestColor):
            return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
        case (BlendFactor::InvSourceColor):
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
        case (BlendFactor::DestAlpha):
            return VK_BLEND_FACTOR_DST_ALPHA;
        case (BlendFactor::SourceAlpha):
            return VK_BLEND_FACTOR_SRC_ALPHA;
        case (BlendFactor::InvDestAlpha):
            return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
        case (BlendFactor::InvSourceAlpha):
            return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        }
    }

    VkBlendOp VulkanUtils::GetBlendOp(BlendFunction blendFunc)
    {
        switch (blendFunc)
        {
        case BlendFunction::ADD:
            return VK_BLEND_OP_ADD;
        case BlendFunction::SUBTRACT:
            return VK_BLEND_OP_SUBTRACT;
        case BlendFunction::REVERSE_SUBTRACT:
            return VK_BLEND_OP_REVERSE_SUBTRACT;
        case BlendFunction::MIN:
            return VK_BLEND_OP_MIN;
        case BlendFunction::MAX:
            return VK_BLEND_OP_MAX;
        }

        return VK_BLEND_OP_ADD;
    }

    VkSampleCountFlagBits VulkanUtils::GetSampleFlags(uint32_t numSamples)
    {
        switch (numSamples)
        {
        case 0:
        case 1:
            return VK_SAMPLE_COUNT_1_BIT;
        case 2:
            return VK_SAMPLE_COUNT_2_BIT;
        case 4:
            return VK_SAMPLE_COUNT_4_BIT;
        case 8:
            return VK_SAMPLE_COUNT_8_BIT;
        case 16:
            return VK_SAMPLE_COUNT_16_BIT;
        case 32:
            return VK_SAMPLE_COUNT_32_BIT;
        case 64:
            return VK_SAMPLE_COUNT_64_BIT;
        }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    VkPrimitiveTopology VulkanUtils::GetDrawFlags(DrawMode drawMode)
    {
        switch (drawMode)
        {
        case DrawMode::TRIANGLE_LIST:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case DrawMode::TRIANGLE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        case DrawMode::TRIANGLE_FAN:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        case DrawMode::POINT_LIST:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case DrawMode::LINE_LIST:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case DrawMode::LINE_STRIP:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        }

        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    VkShaderStageFlagBits VulkanUtils::GetShaderFlags(ShaderType shaderType)
    {
        switch (shaderType)
        {
        case VERTEX_SHADER:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case FRAGMENT_SHADER:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case GEOMETRY_SHADER:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case DOMAIN_SHADER:
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case HULL_SHADER:
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case COMPUTE_SHADER:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        default:
            return VK_SHADER_STAGE_VERTEX_BIT;
        }
    }

    VkPipelineStageFlags VulkanUtils::ShaderToPipelineStage(VkShaderStageFlags shaderStageFlags)
    {
        VkPipelineStageFlags output = 0;
        if ((shaderStageFlags & VK_SHADER_STAGE_VERTEX_BIT) != 0)
            output |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;

        if ((shaderStageFlags & VK_SHADER_STAGE_FRAGMENT_BIT) != 0)
            output |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

        if ((shaderStageFlags & VK_SHADER_STAGE_GEOMETRY_BIT) != 0)
            output |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;

        if ((shaderStageFlags & VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) != 0)
            output |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;

        if ((shaderStageFlags & VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) != 0)
            output |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;

        if ((shaderStageFlags & VK_SHADER_STAGE_COMPUTE_BIT) != 0)
            output |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        return output;
    }

    VkIndexType VulkanUtils::GetIndexType(IndexType indexType)
    {
        switch (indexType)
        {
        case (IndexType::Index_16):
            return VK_INDEX_TYPE_UINT16;
        case (IndexType::Index_32):
            return VK_INDEX_TYPE_UINT32;
        }

        return VK_INDEX_TYPE_UINT32;
    }

    TextureFormat VulkanUtils::GetClosestSupportedTextureFormat(const VulkanDevice& device, TextureFormat format,
                                                                TextureShape shape, int usage, bool optimalTiling)
    {
        VkFormatFeatureFlags wantedFeatureFlags = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((usage & TEXTURE_RENDERTARGET) != 0)
            wantedFeatureFlags |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
        if ((usage & TEXTURE_DEPTHSTENCIL) != 0)
            wantedFeatureFlags |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if ((usage & TEXTURE_LOADSTORE) != 0)
            wantedFeatureFlags |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;

        VkFormatProperties props;
        auto isSupported = [&](VkFormat format) {
            vkGetPhysicalDeviceFormatProperties(device.GetPhysicalDevice(), format, &props);
            VkFormatFeatureFlags featureFlags =
              optimalTiling ? props.optimalTilingFeatures : props.linearTilingFeatures;
            return (featureFlags & wantedFeatureFlags) != 0;
        };

        VkFormat vkFormat = GetTextureFormat(format, false);
        if (!isSupported(vkFormat))
        {
            if ((usage & TEXTURE_DEPTHSTENCIL) != 0)
            {
                bool hasStencil = format == TextureFormat::DEPTH24STENCIL8;
                if (hasStencil)
                {
                    if (isSupported(VK_FORMAT_D24_UNORM_S8_UINT)) // spec guarantees that at least on depth-stencil
                                                                  // format is supported.
                        format = TextureFormat::DEPTH24STENCIL8;  // This should be D32S8 format.
                    else
                        format = TextureFormat::DEPTH24STENCIL8;
                }
                else
                    format = TextureFormat::DEPTH32F; // This should be the D16 format that Vulkan spec guanratees.
            }
            else
            {
                int bitDepths[4];
                PixelUtils::GetBitDepths(format, bitDepths);
                if (bitDepths[0] == 16)
                    format = TextureFormat::RGBA16F;
                else if (bitDepths[0] == 32)
                    format = TextureFormat::RGBA32F;
                else
                    format = TextureFormat::RGBA8;
            }
        }
        return format;
    }

    VkFormat VulkanUtils::GetTextureFormat(TextureFormat format, bool sRGB)
    {
        switch (format)
        {
        case TextureFormat::R8:
            if (sRGB)
                return VK_FORMAT_R8_SRGB;
            return VK_FORMAT_R8_UNORM;
        case TextureFormat::RG8:
            if (sRGB)
                return VK_FORMAT_R8G8_SRGB;
            return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::RGB8:
            if (sRGB)
                return VK_FORMAT_R8G8B8_SRGB;
            return VK_FORMAT_R8G8B8_UNORM;
        case TextureFormat::RGBA8:
            if (sRGB)
                return VK_FORMAT_R8G8B8A8_SRGB;
            return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::RGBA16F:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::RGB32F:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case TextureFormat::RGBA32F:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::RG16F:
            return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::RG32F:
            return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::R32I:
            return VK_FORMAT_R32_SINT;
        case TextureFormat::DEPTH32F:
            return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::DEPTH24STENCIL8:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        default:
            return VK_FORMAT_R8_UNORM;
        }
    }

    bool VulkanUtils::RangeOverlaps(const VkImageSubresourceRange& a, const VkImageSubresourceRange& b)
    {
        int32_t aRight = a.baseArrayLayer + (int32_t)a.layerCount;
        int32_t bRight = b.baseArrayLayer + (int32_t)b.layerCount;

        int32_t aBottom = a.baseMipLevel + (int32_t)a.levelCount;
        int32_t bBottom = b.baseMipLevel + (int32_t)b.levelCount;

        if ((int32_t)a.baseArrayLayer < bRight && aRight > (int32_t)b.baseArrayLayer &&
            (int32_t)a.baseMipLevel < bBottom && aBottom > (int32_t)b.baseMipLevel)
            return true;
        return false;
    }

    void VulkanUtils::CutHorizontal(const VkImageSubresourceRange& toCut, const VkImageSubresourceRange& cutWith,
                                    VkImageSubresourceRange* output, uint32_t& numAreas)
    {
        numAreas = 0;
        int32_t leftCut =
          glm::clamp((int32_t)cutWith.baseArrayLayer - (int32_t)toCut.baseArrayLayer, 0, (int32_t)toCut.layerCount);
        int32_t rightCut =
          glm::clamp((int32_t)(cutWith.baseArrayLayer + cutWith.layerCount) - (int32_t)toCut.baseArrayLayer, 0,
                     (int32_t)toCut.layerCount);
        if (leftCut > 0 && leftCut < (int32_t)toCut.layerCount)
        {
            output[numAreas] = toCut;
            VkImageSubresourceRange& range = output[numAreas];
            range.baseArrayLayer = toCut.baseArrayLayer;
            range.layerCount = leftCut;
            numAreas++;
        }

        if (rightCut > 0 && rightCut < (int32_t)toCut.layerCount)
        {
            output[numAreas] = toCut;
            VkImageSubresourceRange& range = output[numAreas];
            range.baseArrayLayer = toCut.baseArrayLayer + rightCut;
            range.layerCount = toCut.layerCount - rightCut;
            numAreas++;
        }

        if (rightCut > leftCut)
        {
            output[numAreas] = toCut;
            VkImageSubresourceRange& range = output[numAreas];
            range.baseArrayLayer = toCut.baseArrayLayer + leftCut;
            range.layerCount = toCut.layerCount - (toCut.layerCount - rightCut) - leftCut;
            numAreas++;
        }

        if (numAreas == 0)
        {
            output[numAreas] = toCut;
            numAreas++;
        }
    }

    void VulkanUtils::CutVertical(const VkImageSubresourceRange& toCut, const VkImageSubresourceRange& cutWith,
                                  VkImageSubresourceRange* output, uint32_t& numAreas)
    {

        numAreas = 0;
        int32_t topCut =
          glm::clamp((int32_t)cutWith.baseMipLevel - (int32_t)toCut.baseMipLevel, 0, (int32_t)toCut.levelCount);
        int32_t bottomCut =
          glm::clamp((int32_t)(cutWith.baseMipLevel + cutWith.levelCount) - (int32_t)toCut.baseMipLevel, 0,
                     (int32_t)toCut.levelCount);
        if (topCut > 0 && topCut < (int32_t)toCut.levelCount)
        {
            output[numAreas] = toCut;
            VkImageSubresourceRange& range = output[numAreas];
            range.baseMipLevel = toCut.baseMipLevel;
            range.levelCount = topCut;
            numAreas++;
        }

        if (bottomCut > 0 && bottomCut < (int32_t)toCut.levelCount)
        {
            output[numAreas] = toCut;
            VkImageSubresourceRange& range = output[numAreas];
            range.baseMipLevel = toCut.baseMipLevel + bottomCut;
            range.levelCount = toCut.levelCount - bottomCut;
            numAreas++;
        }

        if (bottomCut > topCut)
        {
            output[numAreas] = toCut;
            VkImageSubresourceRange& range = output[numAreas];
            range.baseMipLevel = toCut.baseMipLevel + topCut;
            range.levelCount = toCut.levelCount - (toCut.levelCount - bottomCut) - topCut;
            numAreas++;
        }

        if (numAreas == 0)
        {
            output[numAreas] = toCut;
            numAreas++;
        }
    }

    void VulkanUtils::CutRange(const VkImageSubresourceRange& toCut, const VkImageSubresourceRange& cutWith,
                               std::array<VkImageSubresourceRange, 5>& output, uint32_t& numAreas)
    {
        numAreas = 0;
        uint32_t numHorizontalCuts = 0;
        std::array<VkImageSubresourceRange, 3> horzCuts;
        CutHorizontal(toCut, cutWith, horzCuts.data(), numHorizontalCuts);
        for (uint32_t i = 0; i < numHorizontalCuts; i++)
        {
            VkImageSubresourceRange& range = horzCuts[i];
            if (range.baseArrayLayer >= cutWith.baseArrayLayer &&
                (range.baseArrayLayer + range.layerCount) <= (cutWith.baseArrayLayer + cutWith.layerCount))
            {
                uint32_t numVertCuts = 0;
                CutVertical(range, cutWith, output.data() + numAreas, numVertCuts);
                numAreas += numVertCuts;
            }
            else
            {
                output[numAreas] = range;
                numAreas++;
            }
        }
        CW_ENGINE_ASSERT(numAreas <= 5);
    }

    VkFormat VulkanUtils::GetDummyViewFormat(GpuBufferFormat format)
    {
        switch (format)
        {
        case BF_16x1F:
        case BF_32x1F:
            return VK_FORMAT_R32_SFLOAT;
        case BF_16X2F:
        case BF_32x2F:
            return VK_FORMAT_R16G16_UNORM;
        case BF_32x3F:
        case BF_32x4F:
            return VK_FORMAT_R8G8B8A8_UNORM;

        case BF_16X1U:
        case BF_32X1U:
            return VK_FORMAT_R32_UINT;

        case BF_16X2U:
        case BF_32X2U:
            return VK_FORMAT_R16G16_UINT;

        case BF_16X4U:
        case BF_32X3U:
        case BF_32X4U:
            return VK_FORMAT_R8G8B8A8_UINT;
        default:
            return VK_FORMAT_UNDEFINED;
        }
    }

} // namespace Crowny