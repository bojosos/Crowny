#pragma once

#include "Crowny/Renderer/DrawList2D.h"
#include "Crowny/Renderer/RenderWorld.h"

#include <span>

namespace Crowny
{
    class Texture;
    template <typename T> class FrameVector;

    class RenderHandle2D
    {
    public:
        static constexpr uint32_t IndexBits = 20;
        static constexpr uint32_t MaxInstances = 1u << IndexBits;
        static constexpr uint32_t MaxGeneration = (1u << (32 - IndexBits)) - 1;
        static constexpr RenderHandle2D FromParts(uint32_t index, uint32_t generation)
        {
            return index < MaxInstances && generation > 0 && generation <= MaxGeneration ? RenderHandle2D((generation << IndexBits) | index)
                                                                                         : RenderHandle2D();
        }
        constexpr RenderHandle2D() = default;
        constexpr uint32_t GetIndex() const { return m_Value & (MaxInstances - 1); }
        constexpr uint32_t GetGeneration() const { return m_Value >> IndexBits; }
        constexpr uint32_t GetValue() const { return m_Value; }
        explicit constexpr operator bool() const { return m_Value != 0; }
        bool operator==(const RenderHandle2D&) const = default;

    private:
        explicit constexpr RenderHandle2D(uint32_t value) : m_Value(value) {}
        uint32_t m_Value = 0;
    };

    struct RenderInstance2D
    {
        AffineTransform3x4 Transform;
        AffineTransform3x4 PreviousTransform;
        glm::vec4 Color{ 1.0f };
        glm::vec4 UvRect{ 0.0f, 0.0f, 1.0f, 1.0f };
    };
    static_assert(sizeof(RenderInstance2D) == 128);

    struct RenderInstance2DDesc
    {
        glm::mat4 Transform{ 1.0f };
        glm::vec4 Color{ 1.0f };
        glm::vec4 UvRect{ 0.0f, 0.0f, 1.0f, 1.0f };
        Ref<Texture> TextureResource;
        RenderLayerMask VisibilityLayers = RenderLayerMask::All();
        RenderObjectID ObjectID;
        int32_t SortingLayer = 0;
        int32_t OrderInLayer = 0;
        bool Visible = true;
    };

    enum class RenderChange2DType : uint8_t
    {
        Create,
        Update,
        Destroy,
        Cancelled
    };

    struct RenderChange2D
    {
        RenderHandle2D Handle;
        RenderChange2DType Type = RenderChange2DType::Create;
        RenderInstance2D Data;
        Ref<Texture> TextureResource;
        RenderLayerMask VisibilityLayers;
        RenderObjectID ObjectID;
        int32_t SortingLayer = 0;
        int32_t OrderInLayer = 0;
        bool Visible = true;
        // False retains the renderer's texture. True with a null resource clears
        // it. Explicit native changes default to a complete resource assignment.
        bool TextureChanged = true;
    };

    // Simulation-thread owner. DrainChanges transfers values/resources into a
    // frame snapshot; consumers never read this world's mutable storage.
    class RenderWorld2D final
    {
    public:
        explicit RenderWorld2D(uint32_t capacity = 1024);
        void Reserve(uint32_t capacity);
        void BeginFrame(uint64_t frameNumber);
        RenderHandle2D Create(const RenderInstance2DDesc& desc);
        bool Update(RenderHandle2D handle, const RenderInstance2DDesc& desc);
        bool Destroy(RenderHandle2D handle);
        bool IsAlive(RenderHandle2D handle) const;
        void DrainChanges(Vector<RenderChange2D>& output);
        void DrainChanges(FrameVector<RenderChange2D>& output);
        uint32_t GetActiveCount() const { return m_ActiveCount; }

    private:
        struct Slot
        {
            RenderChange2D Value;
            uint32_t Generation = 1;
            bool PendingChange = false;
            bool Alive = false;
            bool NeedsSettle = false;
        };
        void Queue(uint32_t index, RenderChange2DType type);
        void Assign(Slot& slot, const RenderInstance2DDesc& desc);
        template <typename Append> void DrainTo(Append&& append);

        Vector<Slot> m_Slots;
        Vector<uint32_t> m_Free;
        Vector<uint32_t> m_Retired;
        Vector<uint32_t> m_Moving;
        Vector<uint32_t> m_Changes;
        uint64_t m_FrameNumber = 0;
        uint32_t m_ActiveCount = 0;
        bool m_HasFrame = false;
    };
} // namespace Crowny
