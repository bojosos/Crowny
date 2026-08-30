#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/Types.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Memory/FrameVector.h"
#include "Crowny/RenderAPI/RenderTarget.h"
#include "Crowny/Renderer/DirectionalShadowCascades.h"
#include "Crowny/Renderer/EnvironmentMap.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Renderer/RenderResourceChanges.h"
#include "Crowny/Renderer/RenderTypes.h"
#include "Crowny/Renderer/RenderWorld.h"
#include "Crowny/Renderer/ShadowAtlas.h"

#include <glm/glm.hpp>
#include <limits>
#include <span>

namespace Crowny
{

    struct GridSettings
    {
        float FineSize = 1.0f;
        float CoarseSize = 10.0f;
        float LineWidth = 0.02f;
        float Opacity = 0.4f;
        bool ShowAxes = true;
    };

    struct RenderableObject
    {
        glm::mat4 WorldMatrix = glm::mat4(1.0f);
        // Negative radius means an externally-produced legacy snapshot did not
        // provide culling bounds and must remain visible for compatibility.
        glm::vec4 BoundingSphere = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
        AssetHandle<Mesh> MeshHandle;
        uint32_t MaterialOffset = 0;
        uint32_t MaterialCount = 0;
        RenderLayerMask VisibilityLayers = RenderLayerMask::All();
        bool Visible = true;
    };

    struct RenderableSprite
    {
        glm::mat4 WorldMatrix;
        Ref<Texture> Texture;
        glm::vec4 Color;
        int32_t EntityId;
    };

    struct RenderableText
    {
        TextComponent TextData;
        glm::mat4 WorldMatrix;
        int32_t EntityId;
    };

    enum class Renderable2DType : uint8_t
    {
        Sprite,
        Text
    };

    struct Renderable2DOrder
    {
        Renderable2DType Type = Renderable2DType::Sprite;
        uint32_t Index = 0;
        int32_t SortingLayer = 0;
        int32_t OrderInLayer = 0;
        uint32_t StableOrder = 0;
    };

    inline bool Renderable2DOrderLess(const Renderable2DOrder& first, const Renderable2DOrder& second)
    {
        if (first.SortingLayer != second.SortingLayer)
            return first.SortingLayer < second.SortingLayer;
        if (first.OrderInLayer != second.OrderInLayer)
            return first.OrderInLayer < second.OrderInLayer;
        if (first.StableOrder != second.StableOrder)
            return first.StableOrder < second.StableOrder;
        if (first.Type != second.Type)
            return first.Type < second.Type;
        return first.Index < second.Index;
    }

    struct DirectionalShadowRenderData
    {
        RenderLightHandle Light;
        LightShadowSettings Settings;
        DirectionalShadowCascadeSettings CascadeSettings;
        std::array<DirectionalShadowCascade, 4> Cascades;
        uint32_t CascadeCount = 0;
        bool RequiresRedraw = true;

        bool IsValid() const { return Light.IsValid() && CascadeCount != 0; }
    };

    struct RenderSnapshot
    {
        // Camera
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
        glm::mat4 PreviousViewProjection = glm::mat4(1.0f);
        glm::vec3 CameraPosition;

        // Environment
        Ref<EnvironmentMap> Environment;

        // 3D objects
        FrameVector<RenderableObject> MeshObjects;
        // Flat snapshot-owned material storage avoids one heap allocation per
        // renderable while preserving render-thread ownership of the handles.
        FrameVector<AssetHandle<Material>> LegacyMaterials;

        // Incremental persistent-scene changes consumed by the new renderer.
        // MeshObjects remains the legacy adapter until feature parity is reached.
        FrameVector<RenderWorldChange> RenderWorldChanges;
        FrameVector<RenderLightChange> RenderLightChanges;
        FrameVector<RenderMeshResourceChange> MeshResourceChanges;
        FrameVector<RenderMaterialResourceChange> MaterialResourceChanges;
        FrameVector<uint64_t> ReleasedHistoryNamespaces;
        // Compatibility renderer adapter. The clustered renderer consumes only
        // RenderLightChanges and keeps its light table resident on the GPU.
        FrameVector<RenderLightData> LegacyLights;
        FrameVector<ShadowUpdateRequest> ShadowUpdateRequests;
        DirectionalShadowRenderData DirectionalShadow;

        // 2D objects
        FrameVector<RenderableSprite> Sprites;
        FrameVector<RenderableText> Texts;
        FrameVector<Renderable2DOrder> Ordered2D;

        // Render target
        Ref<RenderTarget> Target;

        // Frame metadata
        uint64_t FrameNumber = 0;
        uint64_t HistoryOwnerId = 0;
        uint64_t HistoryNamespace = 0;
        bool CameraCut = true;
        bool EnableObjectID = false;
        bool EnableMotionVectors = true;
        bool DrawGrid = false;
        GridSettings Grid;
        RenderPipelineSettings PipelineSettings;
        PolygonMode OverridePolygonMode = PolygonMode::Solid;

        static constexpr bool CanAppendMaterials(size_t storedCount, size_t appendCount) noexcept
        {
            constexpr size_t maxMaterialCount = std::numeric_limits<uint32_t>::max();
            return storedCount <= maxMaterialCount && appendCount <= maxMaterialCount - storedCount;
        }

        bool SetMaterials(RenderableObject& object, const Vector<AssetHandle<Material>>& materials)
        {
            const size_t offset = LegacyMaterials.Size();
            if (!CanAppendMaterials(offset, materials.size()))
            {
                object.MaterialOffset = 0;
                object.MaterialCount = 0;
                return false;
            }

            object.MaterialOffset = static_cast<uint32_t>(offset);
            object.MaterialCount = static_cast<uint32_t>(materials.size());
            for (const AssetHandle<Material>& material : materials)
                LegacyMaterials.Acquire() = material;
            return true;
        }

        std::span<const AssetHandle<Material>> GetMaterials(const RenderableObject& object) const noexcept
        {
            const size_t offset = object.MaterialOffset;
            const size_t count = object.MaterialCount;
            if (offset > LegacyMaterials.Size() || count > LegacyMaterials.Size() - offset)
                return {};
            if (count == 0)
                return {};
            return { LegacyMaterials.begin() + offset, count };
        }

        void Clear()
        {
            for (RenderableObject& object : MeshObjects)
            {
                object.MeshHandle = {};
                object.MaterialOffset = 0;
                object.MaterialCount = 0;
            }
            for (AssetHandle<Material>& material : LegacyMaterials)
                material = {};
            for (RenderableSprite& sprite : Sprites)
                sprite.Texture = nullptr;
            for (RenderableText& text : Texts)
            {
                text.TextData.Text.clear();
                text.TextData.Font = {};
            }

            MeshObjects.Reset();
            LegacyMaterials.Reset();
            RenderWorldChanges.Reset();
            RenderLightChanges.Reset();
            for (RenderMeshResourceChange& change : MeshResourceChanges)
                change.Resource = {};
            for (RenderMaterialResourceChange& change : MaterialResourceChanges)
                change.Resource = {};
            MeshResourceChanges.Reset();
            MaterialResourceChanges.Reset();
            ReleasedHistoryNamespaces.Reset();
            LegacyLights.Reset();
            ShadowUpdateRequests.Reset();
            DirectionalShadow = {};
            Sprites.Reset();
            Texts.Reset();
            Ordered2D.Reset();
            Environment = nullptr;
            Target = nullptr;
            FrameNumber = 0;
            HistoryOwnerId = 0;
            HistoryNamespace = 0;
            CameraCut = true;
            EnableObjectID = false;
            EnableMotionVectors = true;
            PreviousViewProjection = glm::mat4(1.0f);
            DrawGrid = false;
            Grid = {};
            PipelineSettings = {};
            OverridePolygonMode = PolygonMode::Solid;
        }
    };

} // namespace Crowny
