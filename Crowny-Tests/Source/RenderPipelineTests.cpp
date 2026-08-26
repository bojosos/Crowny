#include <catch2/catch_test_macros.hpp>

#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/Renderer/RenderPipeline.h"

#include <array>

using namespace Crowny;

namespace
{
    RenderGraphResourceHandle ImportOutput(RenderGraph& graph, uint32_t width = 1920, uint32_t height = 1080)
    {
        RenderGraphTextureDesc desc;
        desc.Width = width;
        desc.Height = height;
        desc.Format = TextureFormat::RGBA8;
        return graph.ImportTexture("Output", desc, 1, RenderGraphResourceState::ColorAttachment, RenderGraphResourceState::Present);
    }
} // namespace

namespace
{
    class TestRenderFeature final : public IRenderFeature
    {
    public:
        RenderGraphInsertionPoint GetInsertionPoint() const override { return RenderGraphInsertionPoint::AfterOpaque; }

        void AddPasses(RenderGraph& graph, RenderView&, RenderBlackboard& blackboard) override
        {
            Called = true;
            const RenderGraphResourceHandle color = blackboard.Get("SceneColor");
            graph.AddPass("TestFeature", RenderGraphQueue::Graphics,
                          [&](RenderGraphPassBuilder& builder) { builder.Read(color, RenderGraphResourceState::ShaderRead); });
        }

        bool Called = false;
    };
} // namespace

TEST_CASE("Render pipeline honors camera path overrides", "[Renderer][Pipeline]")
{
    RenderCapabilities capabilities;
    capabilities.SetCapability(CW_COMPUTE_SHADER);
    capabilities.SetCapability(CW_LOAD_STORE);
    capabilities.IntegratedGpu = true;

    RenderPipelineAsset pipeline;
    CHECK(pipeline.ResolvePath(capabilities) == RenderingPath::ForwardPlus);
    CHECK(pipeline.ResolvePath(capabilities, RenderingPath::DeferredPlus) == RenderingPath::DeferredPlus);
}

TEST_CASE("Render features add passes only at their insertion point", "[Renderer][Pipeline]")
{
    RenderGraph graph;
    RenderBlackboard blackboard;
    RenderView view;
    const RenderGraphResourceHandle color =
      graph.ImportTexture("SceneColor", {}, 1, RenderGraphResourceState::ShaderRead, RenderGraphResourceState::ShaderRead);
    blackboard.Set("SceneColor", color);

    Ref<TestRenderFeature> feature = CreateRef<TestRenderFeature>();
    RenderPipelineAsset pipeline;
    pipeline.AddFeature(feature);
    pipeline.AddFeaturePasses(RenderGraphInsertionPoint::AfterDepth, graph, view, blackboard);
    CHECK_FALSE(feature->Called);

    pipeline.AddFeaturePasses(RenderGraphInsertionPoint::AfterOpaque, graph, view, blackboard);
    CHECK(feature->Called);
    CHECK(graph.Compile().Succeeded);
}

TEST_CASE("Forward Plus frame graph contains the GPU-driven shared pass sequence", "[Renderer][Pipeline]")
{
    RenderGraph graph;
    RenderBlackboard blackboard;
    RenderView view;
    RenderPipelineAsset pipeline;
    RenderPipelineGraphDesc desc;
    desc.Width = 1920;
    desc.Height = 1080;
    desc.Path = RenderingPath::ForwardPlus;
    desc.OutputTarget = ImportOutput(graph);
    desc.DrawBinCount = 3;
    desc.DrawBinLookupCapacity = 8;
    desc.EnableGpuDrawBins = true;
    desc.EnableObjectID = true;

    const RenderPipelineGraphOutput output = pipeline.BuildFrameGraph(graph, view, desc, blackboard);
    const RenderGraphCompileResult& compiled = graph.Compile();

    INFO(compiled.Error);
    REQUIRE(compiled.Succeeded);
    CHECK(compiled.TransientTextureBytes + compiled.TransientBufferBytes <= 256ull * 1024ull * 1024ull);
    CHECK(output.SceneDepth.IsValid());
    CHECK(output.CurrentHiZ.IsValid());
    CHECK(output.HdrColor.IsValid());
    CHECK(output.FinalTarget == desc.OutputTarget);
    CHECK(blackboard.Contains("ClusterLightIndices"));
    CHECK(blackboard.Contains("MeshTable"));
    CHECK(blackboard.Contains("MeshLodTable"));
    CHECK(blackboard.Contains("MeshletTable"));
    CHECK(blackboard.Contains("MeshletCandidates"));
    CHECK(blackboard.Contains("CulledDrawInstances"));
    CHECK(blackboard.Contains("CulledIndirectCommands"));
    CHECK(blackboard.Contains("DrawBinTable"));
    CHECK(blackboard.Contains("IndirectDrawCounts"));
    CHECK(blackboard.Contains("DirectionalLightIndices"));
    CHECK(blackboard.Contains("ClusterLightCounters"));
    CHECK(blackboard.Contains("ShadowAtlas"));
    CHECK(blackboard.Contains("PointShadowArray"));
    CHECK(blackboard.Contains("DirectionalShadowArray"));
    CHECK(blackboard.Contains("ShadowLightTable"));
    CHECK(blackboard.Contains("ShadowViewTable"));
    CHECK(blackboard.Contains("AmbientOcclusion"));
    CHECK(blackboard.Contains("Velocity"));
    CHECK(blackboard.Contains("Bloom"));

    Vector<String> passNames;
    for (RenderGraphPassHandle pass : compiled.PassOrder)
        passNames.push_back(graph.GetPassName(pass));
    CHECK(std::find(passNames.begin(), passNames.end(), "CullInstancesAndSelectLod") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "ExpandVisibleMeshlets") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "LateOcclusionAndMeshletCulling") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "ClearIndirectDrawCounts") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "BinAndCompactIndirectDraws") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "ClearClusterLightCounters") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "BuildClusteredLightLists") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "GTAO") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "ForwardPlusOpaque") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "ForwardPlusTransparencyAndWorld2D") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "TemporalResolve") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "Bloom") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "DeferredGBuffer") == passNames.end());
    CHECK(passNames.back() == "FinalUIComposition");

    const RenderGraphResourceHandle commands = blackboard.Get("IndirectCommands");
    const RenderGraphResourceHandle counts = blackboard.Get("IndirectDrawCounts");
    REQUIRE(commands.IsValid());
    REQUIRE(counts.IsValid());
    CHECK(compiled.Resources[commands.Index].Desc.Buffer.Type == GpuBufferType::IndirectDraw);
    CHECK(compiled.Resources[counts.Index].Desc.Buffer.Type == GpuBufferType::IndirectDraw);
    const auto transitionsToIndirect = [&](RenderGraphResourceHandle resource) {
        return std::any_of(compiled.Barriers.begin(), compiled.Barriers.end(), [&](const RenderGraphBarrier& barrier) {
            return barrier.Resource == resource && barrier.DestinationState == RenderGraphResourceState::IndirectArgument &&
                   graph.GetPassName(barrier.BeforePass) == "ForwardPlusOpaque";
        });
    };
    CHECK(transitionsToIndirect(commands));
    CHECK(transitionsToIndirect(counts));
    CHECK(std::any_of(compiled.Barriers.begin(), compiled.Barriers.end(), [&](const RenderGraphBarrier& barrier) {
        return barrier.Resource == output.ObjectID &&
               barrier.DestinationState == RenderGraphResourceState::ColorAttachmentReadWrite &&
               graph.GetPassName(barrier.BeforePass) == "ForwardPlusOpaque";
    }));
}

TEST_CASE("Depth prepass configures motion-vector and object-ID outputs independently", "[Renderer][Pipeline]")
{
    struct OutputCase
    {
        bool MotionVectors;
        bool ObjectID;
        DepthPrepassOutputMode ExpectedMode;
        uint32_t ExpectedColorAttachmentCount;
        uint32_t ExpectedMotionVectorAttachment;
        uint32_t ExpectedObjectIDAttachment;
    };
    constexpr std::array<OutputCase, 4> cases = {
        OutputCase{ false, false, DepthPrepassOutputMode::DepthOnly, 0, DepthPrepassOutputLayout::NoAttachment,
                    DepthPrepassOutputLayout::NoAttachment },
        OutputCase{ true, false, DepthPrepassOutputMode::MotionVectors, 1, 0, DepthPrepassOutputLayout::NoAttachment },
        OutputCase{ false, true, DepthPrepassOutputMode::ObjectID, 1, DepthPrepassOutputLayout::NoAttachment, 0 },
        OutputCase{ true, true, DepthPrepassOutputMode::MotionVectorsAndObjectID, 2, 0, 1 },
    };

    for (const OutputCase& outputCase : cases)
    {
        CAPTURE(outputCase.MotionVectors, outputCase.ObjectID);
        RenderGraph graph;
        RenderBlackboard blackboard;
        RenderView view;
        RenderPipelineAsset pipeline;
        RenderPipelineGraphDesc desc;
        desc.Width = 320;
        desc.Height = 180;
        desc.OutputTarget = ImportOutput(graph, desc.Width, desc.Height);
        desc.EnableMotionVectors = outputCase.MotionVectors;
        desc.EnableObjectID = outputCase.ObjectID;
        desc.EnablePostProcessing = false;

        const RenderPipelineGraphOutput output = pipeline.BuildFrameGraph(graph, view, desc, blackboard);
        const RenderGraphCompileResult& compiled = graph.Compile();

        INFO(compiled.Error);
        REQUIRE(compiled.Succeeded);
        CHECK(output.DepthPrepassLayout.Mode == outputCase.ExpectedMode);
        CHECK(output.DepthPrepassLayout.ColorAttachmentCount == outputCase.ExpectedColorAttachmentCount);
        CHECK(output.DepthPrepassLayout.MotionVectorAttachment == outputCase.ExpectedMotionVectorAttachment);
        CHECK(output.DepthPrepassLayout.ObjectIDAttachment == outputCase.ExpectedObjectIDAttachment);
        CHECK(blackboard.Contains("Velocity") == outputCase.MotionVectors);
        CHECK(output.ObjectID.IsValid() == outputCase.ObjectID);
        CHECK(blackboard.Contains("ObjectID") == outputCase.ObjectID);
        if (outputCase.ObjectID)
            CHECK(blackboard.Get("ObjectID") == output.ObjectID);
    }
}

TEST_CASE("Deferred Plus frame graph reuses visibility and forward transparency", "[Renderer][Pipeline]")
{
    RenderGraph graph;
    RenderBlackboard blackboard;
    RenderView view;
    RenderPipelineAsset pipeline;
    RenderPipelineGraphDesc desc;
    desc.Width = 1280;
    desc.Height = 720;
    desc.Path = RenderingPath::DeferredPlus;
    desc.OutputTarget = ImportOutput(graph, desc.Width, desc.Height);
    desc.EnableObjectID = true;

    const RenderPipelineGraphOutput output = pipeline.BuildFrameGraph(graph, view, desc, blackboard);
    const RenderGraphCompileResult& compiled = graph.Compile();
    INFO(compiled.Error);
    REQUIRE(compiled.Succeeded);
    CHECK(compiled.TransientTextureBytes + compiled.TransientBufferBytes <= 256ull * 1024ull * 1024ull);

    Vector<String> passNames;
    for (RenderGraphPassHandle pass : compiled.PassOrder)
        passNames.push_back(graph.GetPassName(pass));
    CHECK(std::find(passNames.begin(), passNames.end(), "DeferredGBuffer") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "DeferredPlusLighting8x8") != passNames.end());
    CHECK(std::find(passNames.begin(), passNames.end(), "ForwardPlusTransparencyAndWorld2D") != passNames.end());
    CHECK(blackboard.Contains("GBufferNormalRoughMetal"));
    CHECK(blackboard.Contains("GBufferEmissive"));
    CHECK(blackboard.Contains("GBufferMaterialFlags"));
    CHECK(std::any_of(compiled.Barriers.begin(), compiled.Barriers.end(), [&](const RenderGraphBarrier& barrier) {
        return barrier.Resource == output.ObjectID &&
               barrier.DestinationState == RenderGraphResourceState::ColorAttachmentReadWrite &&
               graph.GetPassName(barrier.BeforePass) == "DeferredGBuffer";
    }));
}

TEST_CASE("Render pipeline cluster buffers follow non-default runtime settings", "[Renderer][Pipeline][Lights][Clusters]")
{
    RenderGraph graph;
    RenderBlackboard blackboard;
    RenderView view;
    RenderPipelineSettings settings;
    settings.ClusterTileSize = 7;
    settings.ClusterDepthSlices = 3;
    settings.MaxLightsPerCluster = 2;
    settings.MaxDirectionalLights = 3;
    RenderPipelineAsset pipeline(settings);
    RenderPipelineGraphDesc desc;
    desc.Width = 17;
    desc.Height = 9;
    desc.OutputTarget = ImportOutput(graph, desc.Width, desc.Height);
    desc.EnablePostProcessing = false;

    pipeline.BuildFrameGraph(graph, view, desc, blackboard);
    const RenderGraphCompileResult& compiled = graph.Compile();
    INFO(compiled.Error);
    REQUIRE(compiled.Succeeded);

    constexpr uint64_t clusterCount = 3ull * 2ull * 3ull;
    const RenderGraphResourceHandle cells = blackboard.Get("ClusterCells");
    const RenderGraphResourceHandle indices = blackboard.Get("ClusterLightIndices");
    const RenderGraphResourceHandle directionals = blackboard.Get("DirectionalLightIndices");
    REQUIRE(cells.IsValid());
    REQUIRE(indices.IsValid());
    REQUIRE(directionals.IsValid());
    CHECK(compiled.Resources[cells.Index].Desc.Buffer.Size == clusterCount * 8ull);
    CHECK(compiled.Resources[indices.Index].Desc.Buffer.Size == clusterCount * 2ull * sizeof(uint32_t));
    CHECK(compiled.Resources[directionals.Index].Desc.Buffer.Size == 3ull * sizeof(uint32_t));
}

TEST_CASE("Render pipeline compatibility bridge executes after its prerequisite", "[Renderer][Pipeline]")
{
    RenderGraph graph;
    RenderBlackboard blackboard;
    RenderView view;
    RenderPipelineAsset pipeline;
    Vector<uint32_t> executionOrder;
    const RenderGraphPassHandle prerequisite = graph.AddPass(
      "ApplyPersistentChanges", RenderGraphQueue::Transfer, [](RenderGraphPassBuilder& builder) { builder.SetSideEffect(); },
      [&](RenderGraphContext&) { executionOrder.push_back(1); });

    RenderPipelineGraphDesc desc;
    desc.Width = 320;
    desc.Height = 180;
    desc.OutputTarget = ImportOutput(graph, desc.Width, desc.Height);
    desc.Prerequisite = prerequisite;
    desc.EnablePostProcessing = false;
    desc.CompatibilityRenderer = [&](RenderGraphContext&) { executionOrder.push_back(2); };
    pipeline.BuildFrameGraph(graph, view, desc, blackboard);

    const RenderGraphCompileResult& compiled = graph.Compile();
    INFO(compiled.Error);
    REQUIRE(compiled.Succeeded);
    REQUIRE(graph.Execute());
    REQUIRE(executionOrder.size() == 2);
    CHECK(executionOrder[0] == 1);
    CHECK(executionOrder[1] == 2);
}
