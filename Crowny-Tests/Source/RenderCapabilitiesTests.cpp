#include <catch2/catch_test_macros.hpp>

#include "Crowny/RenderAPI/RenderCapabilities.h"

using namespace Crowny;

TEST_CASE("RenderCapabilities reports only discovered features", "[Renderer][Capabilities]")
{
    RenderCapabilities capabilities;
    CHECK_FALSE(capabilities.HasCapability(CW_COMPUTE_SHADER));

    capabilities.SetCapability(CW_COMPUTE_SHADER);
    CHECK(capabilities.HasCapability(CW_COMPUTE_SHADER));
    CHECK_FALSE(capabilities.HasCapability(CW_RAY_TRACING));

    capabilities.UnsetCapability(CW_COMPUTE_SHADER);
    CHECK_FALSE(capabilities.HasCapability(CW_COMPUTE_SHADER));
}

TEST_CASE("RenderCapabilities derives renderer tiers from required feature sets", "[Renderer][Capabilities]")
{
    RenderCapabilities capabilities;
    CHECK(capabilities.GetFeatureTier() == RenderFeatureTier::Compatibility);

    capabilities.SetCapability(CW_COMPUTE_SHADER);
    capabilities.SetCapability(CW_MULTI_DRAW_INDIRECT);
    capabilities.SetCapability(CW_SHADER_DRAW_PARAMETERS);
    CHECK(capabilities.GetFeatureTier() == RenderFeatureTier::VulkanBaseline);

    capabilities.SetCapability(CW_DESCRIPTOR_INDEXING);
    capabilities.SetCapability(CW_NON_UNIFORM_TEXTURE_INDEXING);
    capabilities.SetCapability(CW_DRAW_INDIRECT_COUNT);
    CHECK(capabilities.GetFeatureTier() == RenderFeatureTier::GPUDriven);

    capabilities.SetCapability(CW_MESH_SHADER);
    capabilities.SetCapability(CW_RAY_TRACING);
    CHECK(capabilities.GetFeatureTier() == RenderFeatureTier::Future);
}

TEST_CASE("Auto rendering path favors integrated GPU bandwidth", "[Renderer][Capabilities]")
{
    RenderCapabilities capabilities;
    capabilities.SetCapability(CW_COMPUTE_SHADER);
    capabilities.SetCapability(CW_LOAD_STORE);

    capabilities.IntegratedGpu = true;
    CHECK(capabilities.ResolveRenderingPath(RenderingPath::Auto) == RenderingPath::ForwardPlus);

    capabilities.IntegratedGpu = false;
    CHECK(capabilities.ResolveRenderingPath(RenderingPath::Auto) == RenderingPath::DeferredPlus);
    CHECK(capabilities.ResolveRenderingPath(RenderingPath::ForwardPlus) == RenderingPath::ForwardPlus);
}

TEST_CASE("GPU vendor discovery accepts driver vendor strings", "[Renderer][Capabilities]")
{
    CHECK(RenderCapabilities::VendorFromString("NVIDIA Corporation") == GPU_NVIDIA);
    CHECK(RenderCapabilities::VendorFromString("Intel(R) Corporation") == GPU_INTEL);
    CHECK(RenderCapabilities::VendorFromString("Advanced Micro Devices, Inc.") == GPU_AMD);
    CHECK(RenderCapabilities::VendorFromString("Mesa") == GPU_UNKNOWN);
}
