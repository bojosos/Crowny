#pragma once

#include "Crowny/Common/HashedString.h"
#include "Crowny/Common/RefCounted.h"
#include "Crowny/Renderer/RenderGraph.h"
#include "Crowny/Renderer/RenderTypes.h"

namespace Crowny
{
    class RenderCapabilities;

    enum class DepthPrepassOutputMode : uint8_t
    {
        DepthOnly,
        MotionVectors,
        ObjectID,
        MotionVectorsAndObjectID
    };

    struct DepthPrepassOutputLayout
    {
        static constexpr uint32_t NoAttachment = std::numeric_limits<uint32_t>::max();

        DepthPrepassOutputMode Mode = DepthPrepassOutputMode::DepthOnly;
        uint32_t ColorAttachmentCount = 0;
        uint32_t MotionVectorAttachment = NoAttachment;
        uint32_t ObjectIDAttachment = NoAttachment;
    };

    enum class DepthPrepassProgram : uint8_t
    {
        Static,
        Animated,
        StaticObjectID,
        AnimatedObjectID
    };

    struct DepthPrepassProgramSelection
    {
        DepthPrepassProgram Primary = DepthPrepassProgram::Static;
        DepthPrepassProgram Fallback = DepthPrepassProgram::Static;
        bool HasFallback = false;
    };

    DepthPrepassOutputLayout ResolveDepthPrepassOutputLayout(bool enableMotionVectors, bool enableObjectID);
    DepthPrepassProgramSelection ResolveDepthPrepassProgram(DepthPrepassOutputMode outputMode, bool animated);

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
        RenderGraphResourceHandle DrawBinTable;
        RenderGraphResourceHandle DepthInstanceIds;
        RenderGraphResourceHandle DepthIndirectCommands;
        RenderGraphPassHandle Prerequisite;
        // Temporary bridge while legacy drawing is migrated pass by pass. It
        // executes at the forward-only opaque point in both rendering paths.
        RenderGraph::ExecuteCallback CompatibilityRenderer;
        RenderGraph::ExecuteCallback ScheduledShadowRenderer;
        RenderGraph::ExecuteCallback FinalComposition;
        std::function<void(StringView, RenderGraphContext&)> PassExecutor;
        uint32_t DrawBinCount = 0;
        uint32_t DrawBinLookupCapacity = 0;
        bool EnableGpuDrawBins = false;
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
        DepthPrepassOutputLayout DepthPrepassLayout;
    };

    class RenderBlackboard
    {
    public:
        void Set(StringView name, RenderGraphResourceHandle resource);
        bool Contains(StringView name) const;
        RenderGraphResourceHandle Get(StringView name) const;
        void Clear();

    private:
        struct Entry
        {
            RenderGraphResourceHandle Resource;
            uint64_t Generation = 0;
        };

        UnorderedMap<String, Entry, StringHash, StringEqual> m_Resources;
        uint64_t m_Generation = 1;
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
