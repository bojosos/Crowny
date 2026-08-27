#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Renderer/RenderTypes.h"

#include <cstdint>

namespace Crowny
{
    enum class RenderInstanceFlags : uint8_t
    {
        None = 0,
        Visible = 1 << 0,
        CastShadows = 1 << 1,
        ReceiveShadows = 1 << 2,
        MotionVectors = 1 << 3,
        TwoSided = 1 << 4,
        // Forward-only custom materials currently draw their source mesh through
        // ForwardRenderer. Keep GPU depth and visibility on the same LOD until
        // custom passes consume geometry-heap LOD ranges directly.
        ForceLod0 = 1 << 5
    };

    constexpr RenderInstanceFlags operator|(RenderInstanceFlags first, RenderInstanceFlags second)
    {
        return static_cast<RenderInstanceFlags>(static_cast<uint8_t>(first) | static_cast<uint8_t>(second));
    }

    constexpr bool HasFlag(RenderInstanceFlags value, RenderInstanceFlags flag)
    {
        return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
    }

    struct AffineTransform3x4
    {
        glm::vec4 Row0 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec4 Row1 = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        glm::vec4 Row2 = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);

        static AffineTransform3x4 FromMatrix(const glm::mat4& matrix);
        glm::mat4 ToMatrix() const;
    };

    struct RenderTransformRecord
    {
        AffineTransform3x4 Current;
        AffineTransform3x4 Previous;
    };

    struct RenderCullingRecord
    {
        glm::vec4 BoundingSphere = glm::vec4(0.0f);
    };

    // Resource table indices use 24 bits. The upper bytes carry flags and a
    // signed 1/16-step LOD bias, keeping the complete static instance record
    // inside the 128-byte budget.
    struct RenderDrawRecord
    {
        uint32_t MeshAndFlags = 0;
        uint32_t MaterialAndLodBias = 0;
        uint32_t VisibilityLayerMask = 0xffffffffu;
        uint32_t ObjectID = RenderObjectID::InvalidValue;
    };

    struct RenderInstanceData
    {
        RenderTransformRecord Transforms;
        RenderCullingRecord Culling;
        RenderDrawRecord Draw;
    };

    static_assert(sizeof(AffineTransform3x4) == 48, "Affine GPU transforms must occupy three float4 rows");
    static_assert(sizeof(RenderInstanceData) == 128, "Persistent GPU instance metadata must stay within 128 bytes");

    struct RenderInstanceDesc
    {
        glm::mat4 Transform = glm::mat4(1.0f);
        glm::vec4 BoundingSphere = glm::vec4(0.0f);
        uint32_t MeshHandle = 0;
        uint32_t MaterialHandle = 0;
        RenderLayerMask VisibilityLayers = RenderLayerMask::All();
        RenderObjectID ObjectID;
        RenderInstanceFlags Flags = RenderInstanceFlags::Visible | RenderInstanceFlags::CastShadows |
                                    RenderInstanceFlags::ReceiveShadows | RenderInstanceFlags::MotionVectors;
        float LodBias = 0.0f;
    };

    enum class RenderWorldDirtyFlags : uint8_t
    {
        None = 0,
        Transform = 1 << 0,
        Bounds = 1 << 1,
        Draw = 1 << 2,
        All = (1 << 0) | (1 << 1) | (1 << 2)
    };

    constexpr RenderWorldDirtyFlags operator|(RenderWorldDirtyFlags first, RenderWorldDirtyFlags second)
    {
        return static_cast<RenderWorldDirtyFlags>(static_cast<uint8_t>(first) | static_cast<uint8_t>(second));
    }

    enum class RenderWorldChangeType : uint8_t
    {
        Create,
        Update,
        Destroy,
        Cancelled
    };

    struct RenderWorldChange
    {
        RenderInstanceHandle Handle;
        RenderWorldChangeType Type = RenderWorldChangeType::Update;
        RenderWorldDirtyFlags DirtyFlags = RenderWorldDirtyFlags::None;
        RenderInstanceData Data;
    };

    class RenderWorld
    {
    public:
        explicit RenderWorld(uint32_t initialCapacity = 1024);

        RenderInstanceHandle CreateInstance(const RenderInstanceDesc& desc);
        bool UpdateInstance(RenderInstanceHandle handle, const RenderInstanceDesc& desc);
        bool UpdateTransform(RenderInstanceHandle handle, const glm::mat4& transform, const glm::vec4& boundingSphere);
        bool DestroyInstance(RenderInstanceHandle handle);

        bool IsAlive(RenderInstanceHandle handle) const;
        bool TryGetInstance(RenderInstanceHandle handle, RenderInstanceData& output) const;
        uint32_t GetActiveInstanceCount() const;
        uint32_t GetCapacity() const;

        void Reserve(uint32_t capacity);
        void DrainChanges(Vector<RenderWorldChange>& output);

        static uint32_t GetMeshHandle(const RenderDrawRecord& record) { return record.MeshAndFlags & ResourceHandleMask; }
        static uint32_t GetMaterialHandle(const RenderDrawRecord& record) { return record.MaterialAndLodBias & ResourceHandleMask; }
        static RenderInstanceFlags GetFlags(const RenderDrawRecord& record)
        {
            return static_cast<RenderInstanceFlags>(record.MeshAndFlags >> ResourceHandleBits);
        }
        static float GetLodBias(const RenderDrawRecord& record);

    private:
        static constexpr uint32_t ResourceHandleBits = 24;
        static constexpr uint32_t ResourceHandleMask = (1u << ResourceHandleBits) - 1u;
        static constexpr uint32_t InvalidChangeIndex = 0xffffffffu;

        struct Slot
        {
            RenderInstanceData Data;
            uint32_t Generation = 1;
            uint32_t PendingChange = InvalidChangeIndex;
            bool TransformSettleQueued = false;
            bool Alive = false;
        };

        static RenderInstanceData BuildInstanceData(const RenderInstanceDesc& desc, RenderObjectID fallbackObjectID);
        static uint32_t NextGeneration(uint32_t generation);
        static bool TransformsEqual(const AffineTransform3x4& first, const AffineTransform3x4& second);
        void SetTransformSettleRequired(uint32_t slotIndex, bool required);
        void QueueChange(uint32_t slotIndex, RenderInstanceHandle handle, RenderWorldChangeType type,
                         RenderWorldDirtyFlags dirtyFlags);
        bool ValidateHandle(RenderInstanceHandle handle) const;

        mutable Mutex m_Mutex;
        Vector<Slot> m_Slots;
        Vector<uint32_t> m_FreeSlots;
        Vector<uint32_t> m_DeferredFreeSlots;
        Vector<uint32_t> m_TransformSettleSlots;
        Vector<RenderWorldChange> m_PendingChanges;
        uint32_t m_ActiveInstances = 0;
    };

} // namespace Crowny
