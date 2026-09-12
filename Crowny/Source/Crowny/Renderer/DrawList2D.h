#pragma once

#include "Crowny/Common/StdHeaders.h"

#include <span>

namespace Crowny
{
    enum class Primitive2D : uint8_t
    {
        Sprite,
        Circle,
        Glyph
    };

    enum class BatchBreak2D : uint8_t
    {
        First,
        State,
        TextureCapacity,
        InstanceCapacity,
        StoragePage,
        OrderPage,
        Count
    };

    // Resource IDs belong to the owner of the draw list. Zero is the white texture.
    struct DrawItem2D
    {
        uint32_t Instance = 0;
        uint32_t Texture = 0;
        uint32_t Material = 0;
        uint32_t Clip = 0;
        Primitive2D Primitive = Primitive2D::Sprite;
        uint32_t StoragePage = 0;
    };

    struct DrawInstance2D
    {
        uint32_t Instance = 0;
        uint32_t TextureSlot = 0;
        bool operator==(const DrawInstance2D&) const = default;
    };

    struct DrawBatch2D
    {
        uint32_t FirstInstance = 0;
        uint32_t InstanceCount = 0;
        uint32_t Material = 0;
        uint32_t Clip = 0;
        Primitive2D Primitive = Primitive2D::Sprite;
        BatchBreak2D Break = BatchBreak2D::First;
        uint32_t TextureCount = 0;
        // The portable shaders expose at most eight independently bound textures.
        Array<uint32_t, 8> Textures{};
        uint32_t StoragePage = 0;
    };

    // Records an already ordered stream. It never sorts by material, texture, or
    // primitive, and splitting a batch never changes the input order.
    class DrawList2D final
    {
    public:
        explicit DrawList2D(uint32_t textureCapacity = 8, uint32_t instanceCapacity = 65536);
        void Reserve(size_t instances, size_t batches);
        void Clear();
        void Append(const DrawItem2D& item);
        void Append(std::span<const DrawItem2D> items);

        std::span<const DrawInstance2D> GetInstances() const { return m_Instances; }
        std::span<const DrawBatch2D> GetBatches() const { return m_Batches; }
        uint32_t GetTextureCapacity() const { return m_TextureCapacity; }

    private:
        uint32_t m_TextureCapacity;
        uint32_t m_InstanceCapacity;
        Vector<DrawInstance2D> m_Instances;
        Vector<DrawBatch2D> m_Batches;
    };
} // namespace Crowny
