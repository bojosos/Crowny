#pragma once

#include "Crowny/Common/Types.h"

#include <cstdint>

namespace Crowny
{
    enum class RenderingPath : uint8_t
    {
        Auto,
        ForwardPlus,
        DeferredPlus
    };

    enum class AlphaMode : uint8_t
    {
        Opaque,
        Mask,
        Premultiplied,
        Additive,
        WeightedOIT
    };

    enum class RenderFeatureTier : uint8_t
    {
        Compatibility,
        VulkanBaseline,
        GPUDriven,
        Future
    };

    enum class RenderGraphInsertionPoint : uint8_t
    {
        BeforeDepth,
        AfterDepth,
        AfterOpaque,
        BeforeTransparency,
        BeforeTonemap,
        AfterTonemap
    };

    struct RenderLayerMask
    {
        uint32_t Value = 0xffffffffu;

        static constexpr RenderLayerMask All() { return { 0xffffffffu }; }
        static constexpr RenderLayerMask None() { return { 0u }; }
        static constexpr RenderLayerMask FromLayer(uint32_t layer) { return { layer < 32 ? 1u << layer : 0u }; }

        constexpr bool Contains(uint32_t layer) const { return layer < 32 && (Value & (1u << layer)) != 0; }
        constexpr bool Intersects(RenderLayerMask other) const { return (Value & other.Value) != 0; }
        bool operator==(const RenderLayerMask& other) const = default;
    };

    struct RenderObjectID
    {
        static constexpr uint32_t InvalidValue = 0xffffffffu;

        uint32_t Value = InvalidValue;

        bool IsValid() const { return Value != InvalidValue; }
        explicit operator bool() const { return IsValid(); }
        bool operator==(const RenderObjectID& other) const = default;
    };

    class RenderInstanceHandle
    {
    public:
        static constexpr uint32_t IndexBits = 20;
        static constexpr uint32_t GenerationBits = 32 - IndexBits;
        static constexpr uint32_t IndexMask = (1u << IndexBits) - 1u;
        static constexpr uint32_t GenerationMask = (1u << GenerationBits) - 1u;
        static constexpr uint32_t MaxInstances = 1u << IndexBits;

        constexpr RenderInstanceHandle() = default;

        static constexpr RenderInstanceHandle FromParts(uint32_t index, uint32_t generation)
        {
            return index <= IndexMask && generation != 0 && generation <= GenerationMask
                     ? RenderInstanceHandle((generation << IndexBits) | index)
                     : RenderInstanceHandle();
        }

        constexpr bool IsValid() const { return m_Value != 0; }
        constexpr uint32_t GetIndex() const { return m_Value & IndexMask; }
        constexpr uint32_t GetGeneration() const { return m_Value >> IndexBits; }
        constexpr uint32_t GetValue() const { return m_Value; }
        explicit constexpr operator bool() const { return IsValid(); }
        bool operator==(const RenderInstanceHandle& other) const = default;

    private:
        explicit constexpr RenderInstanceHandle(uint32_t value) : m_Value(value) {}

        uint32_t m_Value = 0;
    };

    class RenderLightHandle
    {
    public:
        static constexpr uint32_t IndexBits = 16;
        static constexpr uint32_t GenerationBits = 32 - IndexBits;
        static constexpr uint32_t IndexMask = (1u << IndexBits) - 1u;
        static constexpr uint32_t GenerationMask = (1u << GenerationBits) - 1u;
        static constexpr uint32_t MaxLights = 1u << IndexBits;

        constexpr RenderLightHandle() = default;

        static constexpr RenderLightHandle FromParts(uint32_t index, uint32_t generation)
        {
            return index <= IndexMask && generation != 0 && generation <= GenerationMask
                     ? RenderLightHandle((generation << IndexBits) | index)
                     : RenderLightHandle();
        }

        constexpr bool IsValid() const { return m_Value != 0; }
        constexpr uint32_t GetIndex() const { return m_Value & IndexMask; }
        constexpr uint32_t GetGeneration() const { return m_Value >> IndexBits; }
        constexpr uint32_t GetValue() const { return m_Value; }
        explicit constexpr operator bool() const { return IsValid(); }
        bool operator==(const RenderLightHandle& other) const = default;

    private:
        explicit constexpr RenderLightHandle(uint32_t value) : m_Value(value) {}

        uint32_t m_Value = 0;
    };

    struct MaterialPropertyID
    {
        uint32_t Value = 0;

        bool IsValid() const { return Value != 0; }
        explicit operator bool() const { return IsValid(); }
        bool operator==(const MaterialPropertyID& other) const = default;
    };

    struct RenderPipelineSettings
    {
        RenderingPath Path = RenderingPath::Auto;
        uint32_t FrameContextCount = 2;
        uint32_t ClusterTileSize = 16;
        uint32_t ClusterDepthSlices = 24;
        uint32_t MaxLightsPerCluster = 64;
        uint32_t MaxDirectionalLights = 8;
        uint32_t MaxMeshletCandidates = 1048576;
        uint32_t MaxIndirectCommands = 262144;
        uint32_t MsaaSamples = 1;
        float RenderScale = 1.0f;
        float MinimumDynamicResolutionScale = 0.75f;
        float SharpeningStrength = 0.0f;
        bool EnableDynamicResolution = true;
        bool EnableTaa = true;
        bool EnableGtao = true;
        bool EnableBloom = true;
        bool EnableToonOutlines = true;
    };

    struct RenderView
    {
        glm::mat4 View = glm::mat4(1.0f);
        glm::mat4 Projection = glm::mat4(1.0f);
        glm::mat4 PreviousViewProjection = glm::mat4(1.0f);
        glm::vec2 ViewportSize = glm::vec2(1.0f);
        RenderLayerMask VisibilityMask = RenderLayerMask::All();
        RenderObjectID CameraID;
        RenderingPath Path = RenderingPath::Auto;
        bool CameraCut = true;
        bool EnableObjectID = false;
        bool EnableMotionVectors = true;
    };

} // namespace Crowny
