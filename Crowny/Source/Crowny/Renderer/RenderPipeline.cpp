#include "cwpch.h"

#include "Crowny/Renderer/RenderPipeline.h"

#include "Crowny/RenderAPI/RenderCapabilities.h"
#include "Crowny/Renderer/ClusteredLightGrid.h"
#include "Crowny/Renderer/GpuDrivenDraw.h"
#include "Crowny/Renderer/GpuMaterial.h"

#include <bit>

namespace Crowny
{
    namespace
    {
        RenderGraphTextureDesc Texture2D(uint32_t width, uint32_t height, TextureFormat format, uint32_t mipLevels = 1, uint32_t samples = 1)
        {
            RenderGraphTextureDesc desc;
            desc.Width = std::max(width, 1u);
            desc.Height = std::max(height, 1u);
            desc.MipLevels = std::max(mipLevels, 1u);
            desc.Samples = std::max(samples, 1u);
            desc.Format = format;
            return desc;
        }

        RenderGraphTextureDesc TextureCubeArray(uint32_t size, uint32_t cubeCount, TextureFormat format)
        {
            RenderGraphTextureDesc desc = Texture2D(size, size, format);
            desc.Shape = TextureShape::TEXTURE_CUBE;
            desc.Layers = std::max(cubeCount, 1u) * 6u;
            return desc;
        }

        RenderGraphTextureDesc Texture2DArray(uint32_t width, uint32_t height, uint32_t layers, TextureFormat format)
        {
            RenderGraphTextureDesc desc = Texture2D(width, height, format);
            desc.Layers = std::max(layers, 1u);
            return desc;
        }

        uint32_t HiZMipCount(uint32_t width, uint32_t height) { return std::bit_width(std::max({ width, height, 1u })); }

        RenderGraphBufferDesc StructuredBuffer(uint64_t size, uint32_t stride)
        {
            return { std::max<uint64_t>(size, stride), stride, GpuBufferType::Structured };
        }

        RenderGraphBufferDesc IndirectBuffer(uint64_t size, uint32_t stride)
        {
            return { std::max<uint64_t>(size, stride), stride, GpuBufferType::IndirectDraw };
        }
    } // namespace

    DepthPrepassOutputLayout ResolveDepthPrepassOutputLayout(bool enableMotionVectors, bool enableObjectID)
    {
        if (enableMotionVectors)
        {
            if (enableObjectID)
                return { DepthPrepassOutputMode::MotionVectorsAndObjectID, 2, 0, 1 };
            return { DepthPrepassOutputMode::MotionVectors, 1, 0, DepthPrepassOutputLayout::NoAttachment };
        }
        if (enableObjectID)
            return { DepthPrepassOutputMode::ObjectID, 1, DepthPrepassOutputLayout::NoAttachment, 0 };
        return {};
    }

    DepthPrepassProgramSelection ResolveDepthPrepassProgram(DepthPrepassOutputMode outputMode, bool animated)
    {
        const DepthPrepassProgram standard = animated ? DepthPrepassProgram::Animated : DepthPrepassProgram::Static;
        if (outputMode == DepthPrepassOutputMode::ObjectID)
            return { animated ? DepthPrepassProgram::AnimatedObjectID : DepthPrepassProgram::StaticObjectID, standard, true };
        return { standard, standard, false };
    }

    void RenderBlackboard::Set(StringView name, RenderGraphResourceHandle resource)
    {
        auto entry = m_Resources.find(name);
        if (entry == m_Resources.end())
            entry = m_Resources.emplace(String(name), Entry{}).first;

        entry->second.Resource = resource;
        entry->second.Generation = m_Generation;
    }

    bool RenderBlackboard::Contains(StringView name) const
    {
        const auto entry = m_Resources.find(name);
        return entry != m_Resources.end() && entry->second.Generation == m_Generation;
    }

    RenderGraphResourceHandle RenderBlackboard::Get(StringView name) const
    {
        const auto entry = m_Resources.find(name);
        return entry != m_Resources.end() && entry->second.Generation == m_Generation ? entry->second.Resource : RenderGraphResourceHandle{};
    }

    void RenderBlackboard::Clear()
    {
        if (m_Generation == std::numeric_limits<uint64_t>::max())
        {
            m_Resources.clear();
            m_Generation = 1;
            return;
        }

        m_Generation++;
    }

    RenderingPath RenderPipelineAsset::ResolvePath(const RenderCapabilities& capabilities, RenderingPath cameraOverride) const
    {
        const RenderingPath requested = cameraOverride == RenderingPath::Auto ? m_Settings.Path : cameraOverride;
        return capabilities.ResolveRenderingPath(requested);
    }

    void RenderPipelineAsset::AddFeature(const Ref<IRenderFeature>& feature)
    {
        if (feature == nullptr || std::find(m_Features.begin(), m_Features.end(), feature) != m_Features.end())
            return;
        m_Features.push_back(feature);
    }

    bool RenderPipelineAsset::RemoveFeature(const Ref<IRenderFeature>& feature)
    {
        const auto entry = std::find(m_Features.begin(), m_Features.end(), feature);
        if (entry == m_Features.end())
            return false;
        m_Features.erase(entry);
        return true;
    }

    void RenderPipelineAsset::AddFeaturePasses(RenderGraphInsertionPoint point, RenderGraph& graph, RenderView& view,
                                               RenderBlackboard& blackboard) const
    {
        for (const Ref<IRenderFeature>& feature : m_Features)
        {
            if (feature != nullptr && feature->GetInsertionPoint() == point)
                feature->AddPasses(graph, view, blackboard);
        }
    }

    RenderPipelineGraphOutput RenderPipelineAsset::BuildFrameGraph(RenderGraph& graph, RenderView& view, const RenderPipelineGraphDesc& desc,
                                                                   RenderBlackboard& blackboard) const
    {
        CW_ENGINE_ASSERT(desc.OutputTarget.IsValid(), "A render pipeline frame graph requires an output target");
        RenderPipelineGraphOutput output;
        if (!desc.OutputTarget.IsValid())
            return output;

        const uint32_t width = std::max(desc.Width, 1u);
        const uint32_t height = std::max(desc.Height, 1u);
        const ClusteredLightGridDesc clusterDesc = ClusteredLightBuilder::ResolveDesc(m_Settings, width, height);
        const uint64_t clusterCount = ClusteredLightBuilder::GetClusterCount(clusterDesc);
        auto executePass = [&](RenderPipelinePass pass) -> RenderGraph::ExecuteCallback {
            if (!desc.PassExecutor)
                return {};
            IRenderPipelinePassExecutor* executor = desc.PassExecutor;
            return [executor, pass](RenderGraphContext& context) { executor->Execute(pass, context); };
        };

        const RenderGraphResourceHandle instances =
          desc.InstanceTable.IsValid() ? desc.InstanceTable
                                       : graph.CreateBuffer("PersistentInstances", StructuredBuffer(128, 128), RenderGraphResourceLifetime::History);
        const RenderGraphResourceHandle lights =
          desc.LightTable.IsValid() ? desc.LightTable
                                    : graph.CreateBuffer("PersistentLights", StructuredBuffer(80, 80), RenderGraphResourceLifetime::History);
        const RenderGraphResourceHandle meshes =
          desc.MeshTable.IsValid() ? desc.MeshTable
                                   : graph.CreateBuffer("PersistentMeshes", StructuredBuffer(32, 32), RenderGraphResourceLifetime::History);
        const RenderGraphResourceHandle meshLods =
          desc.MeshLodTable.IsValid() ? desc.MeshLodTable
                                      : graph.CreateBuffer("PersistentMeshLods", StructuredBuffer(16, 16), RenderGraphResourceLifetime::History);
        const RenderGraphResourceHandle meshlets =
          desc.MeshletTable.IsValid() ? desc.MeshletTable
                                      : graph.CreateBuffer("PersistentMeshlets", StructuredBuffer(64, 64), RenderGraphResourceLifetime::History);
        const RenderGraphResourceHandle materials =
          desc.MaterialTable.IsValid() ? desc.MaterialTable
                                       : graph.CreateBuffer("PersistentMaterials", StructuredBuffer(sizeof(GpuMaterialData), sizeof(GpuMaterialData)),
                                                            RenderGraphResourceLifetime::History);
        const RenderGraphResourceHandle drawBinTable =
          desc.DrawBinTable.IsValid()
            ? desc.DrawBinTable
            : graph.CreateBuffer("PersistentDrawBins",
                                 StructuredBuffer(sizeof(GpuDrawBinLookupEntry) * static_cast<uint64_t>(std::max(desc.DrawBinLookupCapacity, 1u)),
                                                  sizeof(GpuDrawBinLookupEntry)),
                                 RenderGraphResourceLifetime::History);
        const RenderGraphHistoryPair hiZHistory =
          graph.CreateHistoryTexture("HiZ", Texture2D(width, height, TextureFormat::R32F, HiZMipCount(width, height)));
        const RenderGraphResourceHandle previousHiZ = hiZHistory.Read;
        const RenderGraphResourceHandle currentHiZ = hiZHistory.Write;
        const RenderGraphResourceHandle visibleInstances =
          graph.CreateBuffer("VisibleInstances", StructuredBuffer(8ull * RenderInstanceHandle::MaxInstances, 8));
        const RenderGraphResourceHandle culledDrawInstances =
          graph.CreateBuffer("CulledDrawInstances",
                             StructuredBuffer(sizeof(GpuVisibleDrawInstance) * static_cast<uint64_t>(std::max(m_Settings.MaxIndirectCommands, 1u)),
                                              sizeof(GpuVisibleDrawInstance)));
        const RenderGraphResourceHandle visibleDrawInstances =
          graph.CreateBuffer("VisibleDrawInstances",
                             StructuredBuffer(sizeof(GpuVisibleDrawInstance) * static_cast<uint64_t>(std::max(m_Settings.MaxIndirectCommands, 1u)),
                                              sizeof(GpuVisibleDrawInstance)));
        const RenderGraphResourceHandle meshletCandidates =
          graph.CreateBuffer("MeshletCandidates", StructuredBuffer(8ull * std::max(m_Settings.MaxMeshletCandidates, 1u), 8));
        const RenderGraphResourceHandle meshletCandidateCounters =
          graph.CreateBuffer("MeshletCandidateCounters", StructuredBuffer(4ull * sizeof(uint32_t), sizeof(uint32_t)));
        const RenderGraphResourceHandle visibilityCounters =
          graph.CreateBuffer("VisibilityCounters", StructuredBuffer(8ull * sizeof(uint32_t), sizeof(uint32_t)));
        const RenderGraphResourceHandle drawCounters =
          graph.CreateBuffer("DrawCounters", StructuredBuffer(8ull * sizeof(uint32_t), sizeof(uint32_t)));
        const RenderGraphResourceHandle drawSortKeys =
          graph.CreateBuffer("DrawSortKeys", StructuredBuffer(16ull * std::max(m_Settings.MaxIndirectCommands, 1u), 16));
        const RenderGraphResourceHandle earlyCommands =
          graph.CreateBuffer("EarlyIndirectCommands", IndirectBuffer(20ull * std::max(m_Settings.MaxIndirectCommands, 1u), 20));
        const RenderGraphResourceHandle culledCommands =
          graph.CreateBuffer("CulledIndirectCommands", StructuredBuffer(20ull * std::max(m_Settings.MaxIndirectCommands, 1u), 20));
        const RenderGraphResourceHandle finalCommands =
          graph.CreateBuffer("FinalIndirectCommands", IndirectBuffer(20ull * std::max(m_Settings.MaxIndirectCommands, 1u), 20));
        const uint32_t drawBinCount = desc.EnableGpuDrawBins ? desc.DrawBinCount : 0u;
        const uint32_t drawBinCounterCount = std::max(drawBinCount * 2u, 2u);
        const RenderGraphResourceHandle drawCounts = graph.CreateBuffer("IndirectDrawCounts", IndirectBuffer(4ull * drawBinCounterCount, 4));
        const RenderGraphResourceHandle depthInstanceIds = desc.DepthInstanceIds.IsValid() ? desc.DepthInstanceIds : visibleInstances;
        const RenderGraphResourceHandle depthCommands = desc.DepthIndirectCommands.IsValid() ? desc.DepthIndirectCommands : earlyCommands;

        output.SceneDepth = graph.CreateTexture("SceneDepth", Texture2D(width, height, TextureFormat::DEPTH32F));
        output.CurrentHiZ = currentHiZ;
        output.HdrColor = graph.CreateTexture("HdrColor", Texture2D(width, height, TextureFormat::RGBA16F));
        output.FinalTarget = desc.OutputTarget;
        output.DepthPrepassLayout = ResolveDepthPrepassOutputLayout(view.EnableMotionVectors, view.EnableObjectID);

        const RenderGraphResourceHandle velocity =
          view.EnableMotionVectors ? graph.CreateTexture("Velocity", Texture2D(width, height, TextureFormat::RG16F)) : RenderGraphResourceHandle{};
        if (view.EnableObjectID)
            output.ObjectID = graph.CreateTexture("ObjectID", Texture2D(width, height, TextureFormat::R32I));

        const RenderGraphResourceHandle shadowAtlas =
          graph.CreateTexture("ShadowAtlas", Texture2D(2048, 2048, TextureFormat::DEPTH32F), RenderGraphResourceLifetime::History);
        const RenderGraphResourceHandle pointShadowArray =
          graph.CreateTexture("PointShadowArray", TextureCubeArray(512, 16, TextureFormat::DEPTH32F), RenderGraphResourceLifetime::History);
        const RenderGraphResourceHandle directionalShadowArray =
          graph.CreateTexture("DirectionalShadowArray", Texture2DArray(2048, 2048, 4, TextureFormat::DEPTH32F), RenderGraphResourceLifetime::History);
        const RenderGraphResourceHandle shadowLightTable =
          graph.CreateBuffer("ShadowLightTable", StructuredBuffer(16ull * RenderLightHandle::MaxLights, 16), RenderGraphResourceLifetime::History);
        const RenderGraphResourceHandle shadowViewTable =
          graph.CreateBuffer("ShadowViewTable", StructuredBuffer(112ull * 4096ull, 112), RenderGraphResourceLifetime::History);
        const RenderGraphResourceHandle clusterCells = graph.CreateBuffer("ClusterCells", StructuredBuffer(clusterCount * 8ull, 8));
        const RenderGraphResourceHandle clusterLightIndices =
          graph.CreateBuffer("ClusterLightIndices", StructuredBuffer(clusterCount * clusterDesc.MaxLightsPerCluster * sizeof(uint32_t), 4));
        const RenderGraphResourceHandle directionalLightIndices =
          graph.CreateBuffer("DirectionalLightIndices", StructuredBuffer(clusterDesc.MaxDirectionalLights * sizeof(uint32_t), 4));
        const RenderGraphResourceHandle clusterCounters =
          graph.CreateBuffer("ClusterLightCounters", StructuredBuffer(4ull * sizeof(uint32_t), sizeof(uint32_t)));
        const RenderGraphResourceHandle ambientOcclusion =
          desc.EnablePostProcessing && m_Settings.EnableGtao
            ? graph.CreateTexture("AmbientOcclusion", Texture2D((width + 1u) / 2u, (height + 1u) / 2u, TextureFormat::R8))
            : RenderGraphResourceHandle{};
        const RenderGraphResourceHandle materialId = m_Settings.EnableToonOutlines && desc.EnableToonOutlines
                                                       ? graph.CreateTexture("MaterialID", Texture2D(width, height, TextureFormat::R32I))
                                                       : RenderGraphResourceHandle{};

        blackboard.Clear();
        blackboard.Set("OutputTarget", desc.OutputTarget);
        blackboard.Set("InstanceTable", instances);
        blackboard.Set("LightTable", lights);
        blackboard.Set("MeshTable", meshes);
        blackboard.Set("MeshLodTable", meshLods);
        blackboard.Set("MeshletTable", meshlets);
        blackboard.Set("MaterialTable", materials);
        blackboard.Set("DrawBinTable", drawBinTable);
        blackboard.Set("SceneDepth", output.SceneDepth);
        blackboard.Set("PreviousHiZ", previousHiZ);
        blackboard.Set("CurrentHiZ", currentHiZ);
        blackboard.Set("HdrColor", output.HdrColor);
        blackboard.Set("SceneColor", output.HdrColor);
        blackboard.Set("VisibleInstances", visibleInstances);
        blackboard.Set("CulledDrawInstances", culledDrawInstances);
        blackboard.Set("VisibleDrawInstances", visibleDrawInstances);
        blackboard.Set("MeshletCandidates", meshletCandidates);
        blackboard.Set("MeshletCandidateCounters", meshletCandidateCounters);
        blackboard.Set("VisibilityCounters", visibilityCounters);
        blackboard.Set("DrawCounters", drawCounters);
        blackboard.Set("IndirectDrawCounts", drawCounts);
        blackboard.Set("DrawSortKeys", drawSortKeys);
        blackboard.Set("CulledIndirectCommands", culledCommands);
        blackboard.Set("IndirectCommands", finalCommands);
        blackboard.Set("DepthInstanceIds", depthInstanceIds);
        blackboard.Set("DepthIndirectCommands", depthCommands);
        blackboard.Set("ClusterCells", clusterCells);
        blackboard.Set("ClusterLightIndices", clusterLightIndices);
        blackboard.Set("DirectionalLightIndices", directionalLightIndices);
        blackboard.Set("ClusterLightCounters", clusterCounters);
        blackboard.Set("ShadowAtlas", shadowAtlas);
        blackboard.Set("PointShadowArray", pointShadowArray);
        blackboard.Set("DirectionalShadowArray", directionalShadowArray);
        blackboard.Set("ShadowLightTable", shadowLightTable);
        blackboard.Set("ShadowViewTable", shadowViewTable);
        if (velocity)
            blackboard.Set("Velocity", velocity);
        if (output.ObjectID)
            blackboard.Set("ObjectID", output.ObjectID);
        if (ambientOcclusion)
            blackboard.Set("AmbientOcclusion", ambientOcclusion);
        if (materialId)
            blackboard.Set("MaterialID", materialId);

        AddFeaturePasses(RenderGraphInsertionPoint::BeforeDepth, graph, view, blackboard);

        const RenderGraphPassHandle clearVisibility = graph.AddPass(
          "ClearVisibilityCounters", RenderGraphQueue::Transfer,
          [&](RenderGraphPassBuilder& builder) {
              if (desc.Prerequisite)
                  builder.DependsOn(desc.Prerequisite);
              builder.Write(visibilityCounters, RenderGraphResourceState::TransferWrite);
          },
          executePass(RenderPipelinePass::ClearVisibilityCounters));
        const RenderGraphPassHandle instanceCulling = graph.AddPass(
          "CullInstancesAndSelectLod", RenderGraphQueue::Compute,
          [&](RenderGraphPassBuilder& builder) {
              builder.DependsOn(clearVisibility);
              builder.Read(instances);
              builder.Read(meshes);
              builder.Read(meshLods);
              builder.Read(previousHiZ);
              builder.Write(visibleInstances);
              builder.Write(earlyCommands);
              builder.Write(visibilityCounters);
          },
          executePass(RenderPipelinePass::CullInstancesAndSelectLod));
        const RenderGraphPassHandle shadowCulling = graph.AddPass(
          "CullShadowViews", RenderGraphQueue::Compute,
          [&](RenderGraphPassBuilder& builder) {
              builder.DependsOn(instanceCulling);
              builder.Read(instances);
              builder.Read(visibleInstances);
              builder.Read(earlyCommands, RenderGraphResourceState::IndirectArgument);
          },
          executePass(RenderPipelinePass::CullShadowViews));
        graph.AddPass(
          "RenderScheduledShadows", RenderGraphQueue::Graphics,
          [&](RenderGraphPassBuilder& builder) {
              builder.DependsOn(shadowCulling);
              builder.Read(earlyCommands, RenderGraphResourceState::IndirectArgument);
              builder.Read(instances);
              builder.Read(materials);
              builder.Write(shadowAtlas, RenderGraphResourceState::DepthWrite);
              builder.Write(pointShadowArray, RenderGraphResourceState::DepthWrite);
              builder.Write(directionalShadowArray, RenderGraphResourceState::DepthWrite);
              builder.Write(shadowLightTable);
              builder.Write(shadowViewTable);
          },
          desc.ScheduledShadowRenderer);
        graph.AddPass(
          "ReverseZDepthVelocity", RenderGraphQueue::Graphics,
          [&](RenderGraphPassBuilder& builder) {
              builder.Read(instances);
              builder.Read(depthInstanceIds);
              builder.Read(depthCommands, RenderGraphResourceState::IndirectArgument);
              builder.Write(output.SceneDepth, RenderGraphResourceState::DepthWrite);
              if (velocity)
                  builder.Write(velocity, RenderGraphResourceState::ColorAttachment);
              if (output.ObjectID)
                  builder.Write(output.ObjectID, RenderGraphResourceState::ColorAttachment);
          },
          executePass(RenderPipelinePass::ReverseZDepthVelocity));

        AddFeaturePasses(RenderGraphInsertionPoint::AfterDepth, graph, view, blackboard);

        graph.AddPass(
          "BuildCurrentHiZ", RenderGraphQueue::Compute,
          [&](RenderGraphPassBuilder& builder) {
              builder.Read(output.SceneDepth, RenderGraphResourceState::DepthRead);
              builder.Write(currentHiZ);
          },
          executePass(RenderPipelinePass::BuildCurrentHiZ));
        const RenderGraphPassHandle clearMeshletCandidates = graph.AddPass(
          "ClearMeshletCandidateCounters", RenderGraphQueue::Transfer,
          [&](RenderGraphPassBuilder& builder) { builder.Write(meshletCandidateCounters, RenderGraphResourceState::TransferWrite); },
          executePass(RenderPipelinePass::ClearMeshletCandidateCounters));
        graph.AddPass(
          "ExpandVisibleMeshlets", RenderGraphQueue::Compute,
          [&](RenderGraphPassBuilder& builder) {
              builder.DependsOn(clearMeshletCandidates);
              builder.Read(instances);
              builder.Read(meshes);
              builder.Read(meshLods);
              builder.Read(visibleInstances);
              builder.Read(visibilityCounters);
              builder.Write(meshletCandidates);
              builder.Write(meshletCandidateCounters);
          },
          executePass(RenderPipelinePass::ExpandVisibleMeshlets));
        const RenderGraphPassHandle clearDrawCounters = graph.AddPass(
          "ClearDrawCounters", RenderGraphQueue::Transfer,
          [&](RenderGraphPassBuilder& builder) { builder.Write(drawCounters, RenderGraphResourceState::TransferWrite); },
          executePass(RenderPipelinePass::ClearDrawCounters));
        const RenderGraphPassHandle clearDrawBinCounts = graph.AddPass(
          "ClearIndirectDrawCounts", RenderGraphQueue::Transfer,
          [&](RenderGraphPassBuilder& builder) { builder.Write(drawCounts, RenderGraphResourceState::TransferWrite); },
          executePass(RenderPipelinePass::ClearIndirectDrawCounts));
        graph.AddPass(
          "LateOcclusionAndMeshletCulling", RenderGraphQueue::Compute,
          [&](RenderGraphPassBuilder& builder) {
              builder.DependsOn(clearDrawCounters);
              builder.Read(instances);
              builder.Read(meshlets);
              builder.Read(currentHiZ);
              builder.Read(meshletCandidates);
              builder.Read(meshletCandidateCounters);
              builder.Write(culledCommands);
              builder.Write(culledDrawInstances);
              builder.Write(drawSortKeys);
              builder.Write(drawCounters);
          },
          executePass(RenderPipelinePass::LateOcclusionAndMeshletCulling));
        graph.AddPass(
          "BinAndCompactIndirectDraws", RenderGraphQueue::Compute,
          [&](RenderGraphPassBuilder& builder) {
              builder.DependsOn(clearDrawBinCounts);
              builder.Read(culledCommands);
              builder.Read(culledDrawInstances);
              builder.Read(drawSortKeys);
              builder.Read(drawCounters);
              builder.Read(materials);
              builder.Read(drawBinTable);
              builder.Write(finalCommands);
              builder.Write(visibleDrawInstances);
              builder.ReadWrite(drawCounts);
          },
          executePass(RenderPipelinePass::BinAndCompactIndirectDraws));
        graph.AddPass(
          "ClearClusterLightCounters", RenderGraphQueue::Compute,
          [&](RenderGraphPassBuilder& builder) {
              builder.Write(clusterCounters);
              builder.Write(directionalLightIndices);
          },
          executePass(RenderPipelinePass::ClearClusterLightCounters));
        graph.AddPass(
          "BuildClusteredLightLists", RenderGraphQueue::Compute,
          [&](RenderGraphPassBuilder& builder) {
              builder.Read(output.SceneDepth, RenderGraphResourceState::DepthRead);
              builder.Read(lights);
              builder.Write(clusterCells);
              builder.Write(clusterLightIndices);
              builder.Write(directionalLightIndices);
              builder.ReadWrite(clusterCounters);
          },
          executePass(RenderPipelinePass::BuildClusteredLightLists));
        if (ambientOcclusion)
        {
            graph.AddPass(
              "GTAO", RenderGraphQueue::Compute,
              [&](RenderGraphPassBuilder& builder) {
                  builder.Read(output.SceneDepth, RenderGraphResourceState::DepthRead);
                  builder.Read(currentHiZ);
                  builder.Write(ambientOcclusion);
              },
              executePass(RenderPipelinePass::Gtao));
        }

        auto readSharedLighting = [&](RenderGraphPassBuilder& builder) {
            builder.Read(instances);
            builder.Read(materials);
            builder.Read(lights);
            builder.Read(depthCommands, RenderGraphResourceState::IndirectArgument);
            builder.Read(depthInstanceIds);
            builder.Read(finalCommands, RenderGraphResourceState::IndirectArgument);
            builder.Read(drawCounts, RenderGraphResourceState::IndirectArgument);
            builder.Read(visibleDrawInstances);
            builder.ReadWrite(output.SceneDepth, RenderGraphResourceState::DepthWrite);
            builder.Read(clusterCells);
            builder.Read(clusterLightIndices);
            builder.Read(directionalLightIndices);
            builder.Read(clusterCounters);
            builder.Read(shadowAtlas, RenderGraphResourceState::DepthRead);
            builder.Read(pointShadowArray, RenderGraphResourceState::DepthRead);
            builder.Read(directionalShadowArray, RenderGraphResourceState::DepthRead);
            builder.Read(shadowLightTable);
            builder.Read(shadowViewTable);
            if (ambientOcclusion)
                builder.Read(ambientOcclusion);
        };

        if (desc.Path == RenderingPath::DeferredPlus)
        {
            const RenderGraphResourceHandle gbufferBaseColor =
              graph.CreateTexture("GBufferBaseColorAO", Texture2D(width, height, TextureFormat::RGBA8));
            const RenderGraphResourceHandle gbufferNormal =
              graph.CreateTexture("GBufferNormalRoughMetal", Texture2D(width, height, TextureFormat::RGBA16F));
            const RenderGraphResourceHandle gbufferEmissive =
              graph.CreateTexture("GBufferEmissive", Texture2D(width, height, TextureFormat::RGBA16F));
            const RenderGraphResourceHandle gbufferMaterialFlags =
              materialId ? materialId : graph.CreateTexture("GBufferMaterialFlags", Texture2D(width, height, TextureFormat::R32I));
            blackboard.Set("GBufferBaseColorAO", gbufferBaseColor);
            blackboard.Set("GBufferNormalRoughMetal", gbufferNormal);
            blackboard.Set("GBufferEmissive", gbufferEmissive);
            blackboard.Set("GBufferMaterialFlags", gbufferMaterialFlags);

            graph.AddPass(
              "DeferredGBuffer", RenderGraphQueue::Graphics,
              [&](RenderGraphPassBuilder& builder) {
                  builder.Read(instances);
                  builder.Read(materials);
                  builder.Read(depthCommands, RenderGraphResourceState::IndirectArgument);
                  builder.Read(depthInstanceIds);
                  builder.Read(finalCommands, RenderGraphResourceState::IndirectArgument);
                  builder.Read(drawCounts, RenderGraphResourceState::IndirectArgument);
                  builder.Read(visibleDrawInstances);
                  builder.ReadWrite(output.SceneDepth, RenderGraphResourceState::DepthWrite);
                  builder.Write(gbufferBaseColor, RenderGraphResourceState::ColorAttachment);
                  builder.Write(gbufferNormal, RenderGraphResourceState::ColorAttachment);
                  builder.Write(gbufferEmissive, RenderGraphResourceState::ColorAttachment);
                  builder.Write(gbufferMaterialFlags, RenderGraphResourceState::ColorAttachment);
                  if (output.ObjectID)
                      builder.ReadWrite(output.ObjectID, RenderGraphResourceState::ColorAttachmentReadWrite);
              },
              executePass(RenderPipelinePass::DeferredGBuffer));
            graph.AddPass(
              "DeferredPlusLighting8x8", RenderGraphQueue::Compute,
              [&](RenderGraphPassBuilder& builder) {
                  builder.Read(gbufferBaseColor);
                  builder.Read(gbufferNormal);
                  builder.Read(gbufferEmissive);
                  builder.Read(gbufferMaterialFlags);
                  builder.Read(materials);
                  builder.Read(lights);
                  builder.Read(clusterCells);
                  builder.Read(clusterLightIndices);
                  builder.Read(directionalLightIndices);
                  builder.Read(clusterCounters);
                  builder.Read(shadowAtlas, RenderGraphResourceState::DepthRead);
                  builder.Read(pointShadowArray, RenderGraphResourceState::DepthRead);
                  builder.Read(directionalShadowArray, RenderGraphResourceState::DepthRead);
                  builder.Read(shadowLightTable);
                  builder.Read(shadowViewTable);
                  if (ambientOcclusion)
                      builder.Read(ambientOcclusion);
                  builder.Write(output.HdrColor);
              },
              executePass(RenderPipelinePass::DeferredPlusLighting));
        }
        else
        {
            graph.AddPass(
              "ForwardPlusOpaque", RenderGraphQueue::Graphics,
              [&](RenderGraphPassBuilder& builder) {
                  readSharedLighting(builder);
                  builder.Write(output.HdrColor, RenderGraphResourceState::ColorAttachment);
                  if (materialId)
                      builder.Write(materialId, RenderGraphResourceState::ColorAttachment);
                  if (output.ObjectID)
                      builder.ReadWrite(output.ObjectID, RenderGraphResourceState::ColorAttachmentReadWrite);
              },
              executePass(RenderPipelinePass::ForwardPlusOpaque));
        }

        graph.AddPass(
          "SkyAndForwardOnlyOpaque", RenderGraphQueue::Graphics,
          [&](RenderGraphPassBuilder& builder) {
              builder.Read(output.SceneDepth, RenderGraphResourceState::DepthRead);
              builder.Write(output.HdrColor, RenderGraphResourceState::ColorAttachmentReadWrite);
          },
          desc.CompatibilityRenderer ? desc.CompatibilityRenderer : executePass(RenderPipelinePass::SkyAndForwardOnlyOpaque));
        if (m_Settings.EnableToonOutlines && desc.EnableToonSilhouettes)
        {
            graph.AddPass(
              "ToonSilhouettes", RenderGraphQueue::Graphics,
              [&](RenderGraphPassBuilder& builder) {
                  builder.Read(instances);
                  builder.Read(materials);
                  builder.Read(depthCommands, RenderGraphResourceState::IndirectArgument);
                  builder.Read(depthInstanceIds);
                  builder.Read(finalCommands, RenderGraphResourceState::IndirectArgument);
                  builder.Read(drawCounts, RenderGraphResourceState::IndirectArgument);
                  builder.Read(visibleDrawInstances);
                  builder.Read(output.SceneDepth, RenderGraphResourceState::DepthRead);
                  builder.Write(output.HdrColor, RenderGraphResourceState::ColorAttachmentReadWrite);
              },
              executePass(RenderPipelinePass::ToonSilhouettes));
        }
        if (m_Settings.EnableToonOutlines && desc.EnableToonOutlines)
        {
            graph.AddPass(
              "ToonOutlines", RenderGraphQueue::Compute,
              [&](RenderGraphPassBuilder& builder) {
                  builder.Read(output.SceneDepth, RenderGraphResourceState::DepthRead);
                  builder.Read(materialId);
                  builder.Read(materials);
                  if (desc.Path == RenderingPath::DeferredPlus)
                      builder.Read(blackboard.Get("GBufferNormalRoughMetal"));
                  builder.ReadWrite(output.HdrColor);
              },
              executePass(RenderPipelinePass::ToonOutlines));
        }
        AddFeaturePasses(RenderGraphInsertionPoint::AfterOpaque, graph, view, blackboard);
        AddFeaturePasses(RenderGraphInsertionPoint::BeforeTransparency, graph, view, blackboard);

        if (desc.EnableTransparency && desc.EnableWeightedOIT)
        {
            const RenderGraphResourceHandle oitAccumulation =
              graph.CreateTexture("OitAccumulation", Texture2D(width, height, TextureFormat::RGBA16F));
            const RenderGraphResourceHandle oitRevealage = graph.CreateTexture("OitRevealage", Texture2D(width, height, TextureFormat::R32F));
            blackboard.Set("OitAccumulation", oitAccumulation);
            blackboard.Set("OitRevealage", oitRevealage);

            graph.AddPass(
              "WeightedOitAccumulation", RenderGraphQueue::Graphics,
              [&](RenderGraphPassBuilder& builder) {
                  builder.Read(instances);
                  builder.Read(materials);
                  builder.Read(depthCommands, RenderGraphResourceState::IndirectArgument);
                  builder.Read(depthInstanceIds);
                  builder.Read(lights);
                  builder.Read(output.SceneDepth, RenderGraphResourceState::DepthRead);
                  builder.Read(clusterCells);
                  builder.Read(clusterLightIndices);
                  builder.Read(directionalLightIndices);
                  builder.Read(clusterCounters);
                  builder.Read(shadowAtlas, RenderGraphResourceState::DepthRead);
                  builder.Read(pointShadowArray, RenderGraphResourceState::DepthRead);
                  builder.Read(directionalShadowArray, RenderGraphResourceState::DepthRead);
                  builder.Read(shadowLightTable);
                  builder.Read(shadowViewTable);
                  if (ambientOcclusion)
                      builder.Read(ambientOcclusion);
                  builder.Write(oitAccumulation, RenderGraphResourceState::ColorAttachment);
                  builder.Write(oitRevealage, RenderGraphResourceState::ColorAttachment);
              },
              executePass(RenderPipelinePass::WeightedOitAccumulation));
            graph.AddPass(
              "WeightedOitComposite", RenderGraphQueue::Compute,
              [&](RenderGraphPassBuilder& builder) {
                  builder.Read(oitAccumulation);
                  builder.Read(oitRevealage);
                  builder.ReadWrite(output.HdrColor);
              },
              executePass(RenderPipelinePass::WeightedOitComposite));
        }

        if (desc.EnableTransparency)
        {
            graph.AddPass(
              "ForwardPlusTransparencyAndWorld2D", RenderGraphQueue::Graphics,
              [&](RenderGraphPassBuilder& builder) {
                  builder.Read(instances);
                  builder.Read(materials);
                  builder.Read(depthCommands, RenderGraphResourceState::IndirectArgument);
                  builder.Read(depthInstanceIds);
                  builder.Read(lights);
                  builder.Read(finalCommands, RenderGraphResourceState::IndirectArgument);
                  builder.Read(visibleDrawInstances);
                  builder.Read(output.SceneDepth, RenderGraphResourceState::DepthRead);
                  builder.Read(clusterCells);
                  builder.Read(clusterLightIndices);
                  builder.Read(directionalLightIndices);
                  builder.Read(clusterCounters);
                  builder.Read(shadowAtlas, RenderGraphResourceState::DepthRead);
                  builder.Read(pointShadowArray, RenderGraphResourceState::DepthRead);
                  builder.Read(directionalShadowArray, RenderGraphResourceState::DepthRead);
                  builder.Read(shadowLightTable);
                  builder.Read(shadowViewTable);
                  if (ambientOcclusion)
                      builder.Read(ambientOcclusion);
                  builder.Write(output.HdrColor, RenderGraphResourceState::ColorAttachmentReadWrite);
              },
              executePass(RenderPipelinePass::ForwardPlusTransparencyAndWorld2D));
        }

        AddFeaturePasses(RenderGraphInsertionPoint::BeforeTonemap, graph, view, blackboard);

        RenderGraphResourceHandle resolved = output.HdrColor;
        if (desc.EnablePostProcessing && m_Settings.EnableTaa && velocity)
        {
            const RenderGraphHistoryPair taaHistory = graph.CreateHistoryTexture("TaaHistory", Texture2D(width, height, TextureFormat::RGBA16F));
            const RenderGraphResourceHandle taaHistoryRead = taaHistory.Read;
            const RenderGraphResourceHandle taaHistoryWrite = taaHistory.Write;
            resolved = graph.CreateTexture("TemporalResolve", Texture2D(width, height, TextureFormat::RGBA16F));
            blackboard.Set("TaaHistoryRead", taaHistoryRead);
            blackboard.Set("TaaHistoryWrite", taaHistoryWrite);
            blackboard.Set("TemporalResolve", resolved);
            graph.AddPass(
              "TemporalResolve", RenderGraphQueue::Compute,
              [&](RenderGraphPassBuilder& builder) {
                  builder.Read(output.HdrColor);
                  builder.Read(output.SceneDepth, RenderGraphResourceState::DepthRead);
                  builder.Read(velocity);
                  builder.Read(taaHistoryRead);
                  builder.Write(taaHistoryWrite);
                  builder.Write(resolved);
              },
              executePass(RenderPipelinePass::TemporalResolve));
        }
        output.ResolvedColor = resolved;
        blackboard.Set("ResolvedColor", resolved);

        RenderGraphResourceHandle bloom;
        if (desc.EnablePostProcessing && m_Settings.EnableBloom)
        {
            bloom = graph.CreateTexture("Bloom", Texture2D((width + 1u) / 2u, (height + 1u) / 2u, TextureFormat::RGBA16F));
            graph.AddPass(
              "Bloom", RenderGraphQueue::Compute,
              [&](RenderGraphPassBuilder& builder) {
                  builder.Read(resolved);
                  builder.Write(bloom);
              },
              executePass(RenderPipelinePass::Bloom));
            blackboard.Set("Bloom", bloom);
        }

        graph.AddPass(
          "ExposureToneMapAndColorGrade", RenderGraphQueue::Graphics,
          [&](RenderGraphPassBuilder& builder) {
              builder.Read(resolved);
              builder.Read(output.SceneDepth, RenderGraphResourceState::DepthRead);
              if (output.ObjectID)
                  builder.Read(output.ObjectID);
              if (bloom)
                  builder.Read(bloom);
              builder.Write(desc.OutputTarget, RenderGraphResourceState::ColorAttachment);
          },
          executePass(RenderPipelinePass::ExposureToneMapAndColorGrade));
        AddFeaturePasses(RenderGraphInsertionPoint::AfterTonemap, graph, view, blackboard);
        graph.AddPass(
          "FinalUIComposition", RenderGraphQueue::Graphics,
          [&](RenderGraphPassBuilder& builder) {
              builder.Write(desc.OutputTarget, RenderGraphResourceState::ColorAttachmentReadWrite);
              builder.SetSideEffect();
          },
          desc.FinalComposition);

        return output;
    }

} // namespace Crowny
