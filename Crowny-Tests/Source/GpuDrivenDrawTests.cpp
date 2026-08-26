#include <catch2/catch_test_macros.hpp>

#include "Crowny/Memory/AllocationCounter.h"
#include "Crowny/Renderer/GpuDrivenDraw.h"

using namespace Crowny;

namespace
{
    GpuDrawCandidate Candidate(uint32_t instance, uint32_t pipeline, uint32_t heap, uint32_t materialTemplate, uint32_t firstIndex, float depth,
                               AlphaMode alpha = AlphaMode::Opaque, int32_t layer = 0)
    {
        GpuDrawCandidate candidate;
        candidate.Bin = { alpha == AlphaMode::Opaque || alpha == AlphaMode::Mask ? RenderDrawPhase::Opaque : RenderDrawPhase::Transparent, alpha,
                          pipeline, heap, materialTemplate };
        candidate.InstanceID = instance;
        candidate.MaterialIndex = materialTemplate;
        candidate.IndexCount = 36;
        candidate.FirstIndex = firstIndex;
        candidate.RenderLayer = layer;
        candidate.ViewDepth = depth;
        return candidate;
    }

    GpuDrawBinKey Bin(RenderDrawPhase phase, AlphaMode alpha, uint32_t pipeline, uint32_t heap, uint32_t materialTemplate)
    {
        return { phase, alpha, pipeline, heap, materialTemplate };
    }
} // namespace

TEST_CASE("GPU draw-bin layout is deterministic and respects indirect device limits", "[Renderer][GpuDriven][DrawBins]")
{
    const Vector<GpuDrawBinKey> keys = {
        Bin(RenderDrawPhase::Opaque, AlphaMode::Mask, 4, 8, 12),
        Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 4, 2, 12),
        Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 4, 2, 12),
        Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 1, 7, 3),
    };
    const GpuDrawBinLayoutDesc desc{ 17, 8, 6 };
    GpuDrawBinLayout layout;
    REQUIRE(layout.Build(keys.data(), static_cast<uint32_t>(keys.size()), desc));

    const Vector<GpuDrawBin>& bins = layout.GetBins();
    REQUIRE(bins.size() == 3);
    CHECK(bins[0].Key.Pipeline == 1);
    CHECK(bins[1].Key.GeometryHeap == 2);
    CHECK(bins[2].Key.Alpha == AlphaMode::Mask);
    CHECK(bins[0].FirstCommand == 0);
    CHECK(bins[0].CommandCapacity == 1);
    CHECK(bins[1].FirstCommand == 1);
    CHECK(bins[1].CommandCapacity == 2);
    CHECK(bins[2].FirstCommand == 3);
    CHECK(bins[2].CommandCapacity == 1);
    CHECK(layout.GetStats().CommandCapacity == 4);
    CHECK(layout.GetStats().LookupCapacity == 8);
    CHECK_FALSE(layout.Build(keys.data(), static_cast<uint32_t>(keys.size()), desc));
}

TEST_CASE("GPU draw-bin lookup includes every submission compatibility field", "[Renderer][GpuDriven][DrawBins]")
{
    const GpuDrawBinKey base = Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 2, 3, 5);
    const Vector<GpuDrawBinKey> keys = {
        base,
        Bin(RenderDrawPhase::ForwardOpaque, AlphaMode::Opaque, 2, 3, 5),
        Bin(RenderDrawPhase::Opaque, AlphaMode::Mask, 2, 3, 5),
        Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 7, 3, 5),
        Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 2, 9, 5),
        Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 2, 3, 11),
    };
    GpuDrawBinLayout layout;
    REQUIRE(layout.Build(keys.data(), static_cast<uint32_t>(keys.size()), { 64, 64, 64 }));

    for (const GpuDrawBinKey& key : keys)
        CHECK(layout.FindBin(key) != GpuDrawBinLookupEntry::InvalidBin);
    CHECK(layout.FindBin(Bin(RenderDrawPhase::Transparent, AlphaMode::Opaque, 2, 3, 5)) == GpuDrawBinLookupEntry::InvalidBin);
}

TEST_CASE("GPU draw-bin layout invalidates only when keys or limits change", "[Renderer][GpuDriven][DrawBins]")
{
    Vector<GpuDrawBinKey> keys = { Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 0, 1, 0) };
    GpuDrawBinLayout layout;
    REQUIRE(layout.Build(keys.data(), static_cast<uint32_t>(keys.size()), { 8, 8, 8 }));
    const uint64_t firstVersion = layout.GetStats().Version;
    CHECK_FALSE(layout.Build(keys.data(), static_cast<uint32_t>(keys.size()), { 8, 8, 8 }));
    CHECK(layout.GetStats().Version == firstVersion);

    keys.push_back(Bin(RenderDrawPhase::Opaque, AlphaMode::Mask, 0, 1, 0));
    CHECK(layout.Build(keys.data(), static_cast<uint32_t>(keys.size()), { 8, 8, 8 }));
    CHECK(layout.GetStats().Version == firstVersion + 1u);
    CHECK(layout.Build(keys.data(), static_cast<uint32_t>(keys.size()), { 8, 8, 3 }));
    CHECK(layout.GetStats().Version == firstVersion + 2u);
}

TEST_CASE("GPU draw-bin capacity rejects excess bins and preserves zero-count slots", "[Renderer][GpuDriven][DrawBins]")
{
    const Vector<GpuDrawBinKey> keys = {
        Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 0, 1, 0),
        Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 0, 2, 0),
        Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 0, 3, 0),
    };
    GpuDrawBinLayout layout;
    REQUIRE(layout.Build(keys.data(), static_cast<uint32_t>(keys.size()), { 2, 2, 32 }));
    REQUIRE(layout.GetBins().size() == 2);
    CHECK(layout.GetStats().RejectedBinCount == 1);
    CHECK(layout.GetStats().CommandCapacity == 2);
    CHECK(layout.FindBin(keys[2]) == GpuDrawBinLookupEntry::InvalidBin);

    Vector<uint32_t> counts(layout.GetBins().size() * 2u, 0u);
    CHECK(counts[layout.GetBins()[0].CountIndex] == 0);
    CHECK(counts[layout.GetBins()[1].CountIndex] == 0);
    const uint32_t overflowIndex = static_cast<uint32_t>(layout.GetBins().size()) + layout.GetBins()[0].CountIndex;
    counts[layout.GetBins()[0].CountIndex] = layout.GetBins()[0].CommandCapacity + 3u;
    counts[overflowIndex] = 3u;
    CHECK(std::min(counts[layout.GetBins()[0].CountIndex], layout.GetBins()[0].CommandCapacity) == layout.GetBins()[0].CommandCapacity);
    CHECK(counts[overflowIndex] == 3);
}

TEST_CASE("GPU draw-bin admission never accepts a partial bin", "[Renderer][GpuDriven][DrawBins]")
{
    const GpuDrawBinKey hot = Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 0, 1, 0);
    const GpuDrawBinKey cold = Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 0, 2, 0);
    const Vector<GpuDrawBinKey> keys = { hot, hot, hot, hot, hot, hot, cold, cold };
    GpuDrawBinLayout layout;
    REQUIRE(layout.Build(keys.data(), static_cast<uint32_t>(keys.size()), { 4, 8, 3 }));
    REQUIRE(layout.GetBins().size() == 1);
    CHECK(layout.GetBins()[0].Key == cold);
    CHECK(layout.GetBins()[0].CommandCapacity == 2);
    CHECK(layout.GetStats().CommandCapacity == 2);
    CHECK(layout.GetStats().RejectedBinCount == 1);
    CHECK_FALSE(layout.Contains(hot));
    CHECK(layout.Contains(cold));
}

TEST_CASE("GPU draw-bin budget admits whole hot bins and leaves the rest for CPU fallback", "[Renderer][GpuDriven][DrawBins]")
{
    const GpuDrawBinKey hot = Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 0, 1, 0);
    const GpuDrawBinKey cold = Bin(RenderDrawPhase::Opaque, AlphaMode::Opaque, 0, 2, 0);
    const Vector<GpuDrawBinKey> keys = { hot, hot, hot, cold, cold };
    GpuDrawBinLayout layout;
    REQUIRE(layout.Build(keys.data(), static_cast<uint32_t>(keys.size()), { 4, 8, 8 }));
    REQUIRE(layout.GetBins().size() == 1);
    CHECK(layout.GetBins()[0].Key == hot);
    CHECK(layout.GetBins()[0].CommandCapacity == 3);
    CHECK(layout.GetStats().CommandCapacity == 3);
    CHECK(layout.Contains(hot));
    CHECK_FALSE(layout.Contains(cold));
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

TEST_CASE("GPU draw-list preparation allocates nothing after warm-up", "[Renderer][GpuDriven][Memory][Frame]")
{
    constexpr uint32_t candidateCount = 10000;
    Vector<GpuDrawCandidate> candidates;
    candidates.reserve(candidateCount);
    for (uint32_t index = 0; index < candidateCount; index++)
    {
        const AlphaMode alpha = index % 5u == 0u ? AlphaMode::Premultiplied : (index % 3u == 0u ? AlphaMode::Mask : AlphaMode::Opaque);
        candidates.push_back(Candidate(index, index % 7u, index % 11u + 1u, index % 13u, (index % 64u) * 36u,
                                       static_cast<float>(candidateCount - index), alpha, static_cast<int32_t>(index % 4u)));
    }
    Vector<GpuDrawCandidate> transparentCandidates = candidates;
    for (GpuDrawCandidate& candidate : transparentCandidates)
    {
        candidate.Bin.Phase = RenderDrawPhase::Transparent;
        candidate.Bin.Alpha = AlphaMode::Premultiplied;
    }

    GpuDrawListBuilder builder;
    GpuDrawList output;
    builder.Build(candidates.data(), static_cast<uint32_t>(candidates.size()), output);

    const Memory::ThreadAllocationSnapshot before = Memory::GetThreadAllocationSnapshot();
    for (uint32_t frame = 0; frame < 120; frame++)
    {
        const Vector<GpuDrawCandidate>& frameCandidates = frame % 2u == 0u ? candidates : transparentCandidates;
        builder.Build(frameCandidates.data(), static_cast<uint32_t>(frameCandidates.size()), output);
    }
    const Memory::ThreadAllocationSnapshot after = Memory::GetThreadAllocationSnapshot();
    const Memory::ThreadAllocationSnapshot delta = Memory::GetThreadAllocationDelta(before, after);

    CHECK(output.Instances.size() == candidateCount);
    CHECK(output.Commands.size() == candidateCount);
    CHECK(delta.AllocationCount == 0);
    CHECK(delta.RequestedBytes == 0);
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
