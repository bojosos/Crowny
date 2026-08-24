#pragma once

#include "Crowny/Common/Types.h"
#include "Crowny/Renderer/RenderTypes.h"

namespace Crowny
{
    enum class Gpu2DPrimitive : uint8_t
    {
        Sprite,
        Circle,
        Glyph,
        Line,
        ParticleSprite
    };

    enum class Gpu2DOrderingMode : uint8_t
    {
        StableLayers,
        BatchOptimized
    };

    // One record per quad instead of four expanded CPU vertices. Glyph UVs and
    // circle parameters share the same record and one immutable unit quad.
    struct Gpu2DInstanceData
    {
        glm::mat4 Transform = glm::mat4(1.0f);
        glm::vec4 Color = glm::vec4(1.0f);
        glm::vec4 UvRect = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        glm::vec4 Parameters = glm::vec4(0.0f);
        // texture index, object ID, primitive kind, feature flags
        glm::uvec4 Metadata = glm::uvec4(0u);
    };

    static_assert(sizeof(Gpu2DInstanceData) == 128, "2D instance records must preserve std430 alignment");

    struct Gpu2DBatchKey
    {
        uint32_t Pipeline = 0;
        uint32_t MaterialTemplate = 0;
        AlphaMode Alpha = AlphaMode::Premultiplied;
        Gpu2DPrimitive Primitive = Gpu2DPrimitive::Sprite;

        bool operator==(const Gpu2DBatchKey& other) const = default;
    };

    struct Gpu2DDrawItem
    {
        Gpu2DInstanceData Data;
        Gpu2DBatchKey Batch;
        int32_t SortingLayer = 0;
        int32_t OrderInLayer = 0;
        uint32_t StableOrder = 0;
    };

    struct Gpu2DBatchRun
    {
        Gpu2DBatchKey Batch;
        uint32_t FirstInstance = 0;
        uint32_t InstanceCount = 0;
    };

    struct Gpu2DDrawList
    {
        Vector<Gpu2DInstanceData> Instances;
        Vector<Gpu2DBatchRun> Runs;

        void Clear()
        {
            Instances.clear();
            Runs.clear();
        }
    };

    // CPU reference and OpenGL fallback for the GPU radix-sort/batch contract.
    // StableLayers only merges adjacent compatible items, preserving exact 2D
    // ordering. BatchOptimized is intended for weighted-OIT particles.
    class Gpu2DBatchBuilder
    {
    public:
        void Reserve(uint32_t itemCount);
        void Build(const Gpu2DDrawItem* items, uint32_t itemCount, Gpu2DOrderingMode ordering, Gpu2DDrawList& output);

    private:
        static bool BatchLess(const Gpu2DBatchKey& first, const Gpu2DBatchKey& second);

        Vector<uint32_t> m_Order;
    };
} // namespace Crowny
