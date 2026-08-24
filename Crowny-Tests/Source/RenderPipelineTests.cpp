#include <catch2/catch_test_macros.hpp>

#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/Renderer/RenderPipeline.h"

using namespace Crowny;

namespace
{
    RenderGraphResourceHandle ImportOutput(RenderGraph& graph, uint32_t width = 1920, uint32_t height = 1080)
    {
        RenderGraphTextureDesc desc;
        desc.Width = width;
        desc.Height = height;
        desc.Format = TextureFormat::RGBA8;
        return graph.ImportTexture("Output", desc, 1, RenderGraphResourceState::ColorAttachment,
                                   RenderGraphResourceState::Present);
    }
}

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
}

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
    const RenderGraphResourceHandle color = graph.ImportTexture("SceneColor", {}, 1, RenderGraphResourceState::ShaderRead,
                                                                RenderGraphResourceState::ShaderRead);
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

    pipeline.BuildFrameGraph(graph, view, desc, blackboard);
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
}

TEST_CASE("Render pipeline compatibility bridge executes after its prerequisite", "[Renderer][Pipeline]")
{
    RenderGraph graph;
    RenderBlackboard blackboard;
    RenderView view;
    RenderPipelineAsset pipeline;
    Vector<uint32_t> executionOrder;
    const RenderGraphPassHandle prerequisite = graph.AddPass(
      "ApplyPersistentChanges", RenderGraphQueue::Transfer,
      [](RenderGraphPassBuilder& builder) { builder.SetSideEffect(); },
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
