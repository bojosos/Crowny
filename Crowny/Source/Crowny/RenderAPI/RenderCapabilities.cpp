#include "cwpch.h"

#include "Crowny/RenderAPI/RenderCapabilities.h"

namespace Crowny
{
    char const* const RenderCapabilities::GPU_VENDOR_STRINGS[GPU_VENDOR_COUNT] = { "unknown", "nvidia", "amd", "intel" };

    GPUVendor RenderCapabilities::VendorFromString(const String& vendorString)
    {
        String normalized = vendorString;
        StringUtils::ToLower(normalized);
        if (normalized.find("nvidia") != String::npos)
            return GPU_NVIDIA;
        if (normalized.find("intel") != String::npos)
            return GPU_INTEL;
        if (normalized.find("amd") != String::npos || normalized.find("ati") != String::npos ||
            normalized.find("advanced micro devices") != String::npos)
            return GPU_AMD;
        return GPU_UNKNOWN;
    }

    String RenderCapabilities::VendorToString(GPUVendor vendor)
    {
        return vendor >= GPU_UNKNOWN && vendor < GPU_VENDOR_COUNT ? GPU_VENDOR_STRINGS[vendor] : GPU_VENDOR_STRINGS[GPU_UNKNOWN];
    }

    RenderFeatureTier RenderCapabilities::GetFeatureTier() const
    {
        const bool baseline = HasCapability(CW_COMPUTE_SHADER) && HasCapability(CW_MULTI_DRAW_INDIRECT) &&
                              HasCapability(CW_SHADER_DRAW_PARAMETERS);
        const bool gpuDriven = baseline && HasCapability(CW_DESCRIPTOR_INDEXING) &&
                               HasCapability(CW_NON_UNIFORM_TEXTURE_INDEXING) && HasCapability(CW_DRAW_INDIRECT_COUNT);

        if (gpuDriven && HasCapability(CW_MESH_SHADER) && HasCapability(CW_RAY_TRACING))
            return RenderFeatureTier::Future;
        if (gpuDriven)
            return RenderFeatureTier::GPUDriven;
        if (baseline)
            return RenderFeatureTier::VulkanBaseline;
        return RenderFeatureTier::Compatibility;
    }

    RenderingPath RenderCapabilities::ResolveRenderingPath(RenderingPath requested) const
    {
        const bool supportsClusteredLighting = HasCapability(CW_COMPUTE_SHADER) && HasCapability(CW_LOAD_STORE);
        if (!supportsClusteredLighting)
            return RenderingPath::ForwardPlus;

        if (requested != RenderingPath::Auto)
            return requested;

        if (RenderAPIName == "OpenGL")
            return RenderingPath::ForwardPlus;
        return IntegratedGpu ? RenderingPath::ForwardPlus : RenderingPath::DeferredPlus;
    }
} // namespace Crowny
