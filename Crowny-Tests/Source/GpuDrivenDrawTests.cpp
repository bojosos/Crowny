#include <catch2/catch_test_macros.hpp>

#include "Crowny/Renderer/GpuDrivenDraw.h"

using namespace Crowny;

namespace
{
    GpuDrawCandidate Candidate(uint32_t instance, uint32_t pipeline, uint32_t heap, uint32_t materialTemplate,
                               uint32_t firstIndex, float depth, AlphaMode alpha = AlphaMode::Opaque, int32_t layer = 0)
    {
        GpuDrawCandidate candidate;
        candidate.Bin = { alpha == AlphaMode::Opaque || alpha == AlphaMode::Mask ? RenderDrawPhase::Opaque
                                                                                : RenderDrawPhase::Transparent,
                          alpha, pipeline, heap, materialTemplate };
        candidate.InstanceID = instance;
        candidate.MaterialIndex = materialTemplate;
        candidate.IndexCount = 36;
        candidate.FirstIndex = firstIndex;
        candidate.RenderLayer = layer;
        candidate.ViewDepth = depth;
        return candidate;
    }
}

TEST_CASE("GPU draw generation batches material records sharing a template", "[Renderer][GpuDriven]")
{
    Vector<GpuDrawCandidate> candidates = {
        Candidate(7, 2, 1, 9, 0, 30.0f),
        Candidate(3, 2, 1, 9, 0, 10.0f),
        Candidate(5, 2, 1, 9, 120, 20.0f),
        Candidate(8, 4, 1, 2, 0, 15.0f),
    };

    GpuDrawListBuilder builder;
    GpuDrawList output;
    builder.Build(candidates.data(), static_cast<uint32_t>(candidates.size()), output);

    REQUIRE(output.Runs.size() == 2);
    REQUIRE(output.Commands.size() == 3);
    REQUIRE(output.Instances.size() == 4);
    CHECK(output.Commands[0].InstanceCount == 2);
    CHECK(output.Commands[0].FirstInstance == 0);
    CHECK(output.Instances[0].InstanceID == 3);
    CHECK(output.Instances[0].MaterialIndex == 9);
    CHECK(output.Instances[1].InstanceID == 7);
    CHECK(output.Instances[1].MaterialIndex == 9);
    CHECK(output.Runs[0].CommandCount == 2);
}

TEST_CASE("Strict transparent draw generation preserves render-layer and depth order", "[Renderer][GpuDriven]")
{
    Vector<GpuDrawCandidate> candidates = {
        Candidate(1, 1, 1, 1, 0, 2.0f, AlphaMode::Premultiplied, 3),
        Candidate(2, 2, 1, 2, 0, 9.0f, AlphaMode::Premultiplied, 3),
        Candidate(3, 1, 1, 1, 0, 4.0f, AlphaMode::Additive, 2),
        Candidate(4, 1, 1, 1, 0, 7.0f, AlphaMode::Premultiplied, 3),
    };

    GpuDrawListBuilder builder;
    GpuDrawList output;
    builder.Build(candidates.data(), static_cast<uint32_t>(candidates.size()), output);

    REQUIRE(output.Commands.size() == 4);
    REQUIRE(output.Instances.size() == 4);
    CHECK(output.Instances[0].InstanceID == 3);
    CHECK(output.Instances[1].InstanceID == 2);
    CHECK(output.Instances[2].InstanceID == 4);
    CHECK(output.Instances[3].InstanceID == 1);
    CHECK(output.StrictTransparentCommandCount == 4);
    for (const DrawIndexedIndirectCommand& command : output.Commands)
        CHECK(command.InstanceCount == 1);
}

TEST_CASE("Weighted OIT draws are binned and instanced", "[Renderer][GpuDriven]")
{
    Vector<GpuDrawCandidate> candidates = {
        Candidate(10, 6, 2, 4, 30, 100.0f, AlphaMode::WeightedOIT),
        Candidate(11, 6, 2, 4, 30, 1.0f, AlphaMode::WeightedOIT),
    };

    GpuDrawListBuilder builder;
    GpuDrawList output;
    builder.Build(candidates.data(), static_cast<uint32_t>(candidates.size()), output);

    REQUIRE(output.Commands.size() == 1);
    CHECK(output.Commands[0].InstanceCount == 2);
    CHECK(output.StrictTransparentCommandCount == 0);
}

TEST_CASE("GPU draw buffers have a headless compatibility mode", "[Renderer][GpuDriven]")
{
    GpuDrawList output;
    output.Instances = { { 1, 3 }, { 2, 4 } };
    output.Commands = { { 3, 2, 0, 0, 0 } };
    output.Runs = { { {}, 0, 1 } };

    GpuDrawBuffers buffers(false);
    buffers.Upload(output);
    CHECK_FALSE(buffers.HasGpuBuffers());
    CHECK(buffers.GetStats().UploadedBytes == 0);
}
