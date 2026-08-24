#pragma once

#include "Crowny/Common/RefCounted.h"
#include "Crowny/Renderer/RenderGraph.h"
#include "Crowny/Renderer/RenderTypes.h"

namespace Crowny
{
    class RenderCapabilities;

    struct RenderPipelineGraphDesc
    {
        uint32_t Width = 1;
        uint32_t Height = 1;
        RenderingPath Path = RenderingPath::ForwardPlus;
        RenderGraphResourceHandle OutputTarget;
        RenderGraphResourceHandle InstanceTable;
        RenderGraphResourceHandle LightTable;
        RenderGraphResourceHandle MeshTable;
        RenderGraphResourceHandle MeshLodTable;
        RenderGraphResourceHandle MeshletTable;
        RenderGraphResourceHandle MaterialTable;
        RenderGraphResourceHandle DepthInstanceIds;
        RenderGraphResourceHandle DepthIndirectCommands;
        RenderGraphPassHandle Prerequisite;
        // Temporary bridge while legacy drawing is migrated pass by pass. It
        // executes at the forward-only opaque point in both rendering paths.
        RenderGraph::ExecuteCallback CompatibilityRenderer;
        RenderGraph::ExecuteCallback ScheduledShadowRenderer;
        RenderGraph::ExecuteCallback FinalComposition;
        std::function<void(StringView, RenderGraphContext&)> PassExecutor;
        bool EnableMotionVectors = true;
        bool EnableObjectID = false;
        bool EnableTransparency = true;
        bool EnablePostProcessing = true;
    };

    struct RenderPipelineGraphOutput
    {
        RenderGraphResourceHandle SceneDepth;
        RenderGraphResourceHandle CurrentHiZ;
        RenderGraphResourceHandle HdrColor;
        RenderGraphResourceHandle ResolvedColor;
        RenderGraphResourceHandle ObjectID;
        RenderGraphResourceHandle FinalTarget;
    };

    class RenderBlackboard
    {
    public:
        void Set(const String& name, RenderGraphResourceHandle resource) { m_Resources.insert_or_assign(name, resource); }
        bool Contains(const String& name) const { return m_Resources.find(name) != m_Resources.end(); }
        RenderGraphResourceHandle Get(const String& name) const;
        void Clear() { m_Resources.clear(); }

    private:
        UnorderedMap<String, RenderGraphResourceHandle> m_Resources;
    };

    class IRenderFeature : public RefCounted
    {
    public:
        virtual ~IRenderFeature() = default;
        virtual RenderGraphInsertionPoint GetInsertionPoint() const = 0;
        virtual void AddPasses(RenderGraph& graph, RenderView& view, RenderBlackboard& blackboard) = 0;
    };

    // Runtime representation of the project renderer configuration. Asset
    // serialization is intentionally kept outside the render thread and will
    // wrap this type when the project settings migration lands.
    class RenderPipelineAsset : public RefCounted
    {
    public:
        explicit RenderPipelineAsset(const RenderPipelineSettings& settings = {}) : m_Settings(settings) {}

        const RenderPipelineSettings& GetSettings() const { return m_Settings; }
        void SetSettings(const RenderPipelineSettings& settings) { m_Settings = settings; }
        RenderingPath ResolvePath(const RenderCapabilities& capabilities, RenderingPath cameraOverride = RenderingPath::Auto) const;

        void AddFeature(const Ref<IRenderFeature>& feature);
        bool RemoveFeature(const Ref<IRenderFeature>& feature);
        const Vector<Ref<IRenderFeature>>& GetFeatures() const { return m_Features; }
        void AddFeaturePasses(RenderGraphInsertionPoint point, RenderGraph& graph, RenderView& view, RenderBlackboard& blackboard) const;
        RenderPipelineGraphOutput BuildFrameGraph(RenderGraph& graph, RenderView& view, const RenderPipelineGraphDesc& desc,
                                                  RenderBlackboard& blackboard) const;

    private:
        RenderPipelineSettings m_Settings;
        Vector<Ref<IRenderFeature>> m_Features;
    };

} // namespace Crowny
