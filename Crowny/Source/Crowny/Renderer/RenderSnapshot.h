#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/Types.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/RenderAPI/RenderTarget.h"
#include "Crowny/Renderer/EnvironmentMap.h"
#include "Crowny/Renderer/Material.h"
#include "Crowny/Renderer/Mesh.h"

#include <glm/glm.hpp>

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
        glm::mat4 WorldMatrix;
        AssetHandle<Mesh> MeshHandle;
        Vector<AssetHandle<Material>> Materials;
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

    struct RenderSnapshot
    {
        // Camera
        glm::mat4 ViewMatrix;
        glm::mat4 ProjectionMatrix;
        glm::vec3 CameraPosition;

        // Environment
        Ref<EnvironmentMap> Environment;

        // 3D objects
        Vector<RenderableObject> MeshObjects;

        // 2D objects
        Vector<RenderableSprite> Sprites;
        Vector<RenderableText> Texts;

        // Render target
        Ref<RenderTarget> Target;

        // Frame metadata
        uint64_t FrameNumber = 0;
        bool DrawGrid = false;
        GridSettings Grid;
        PolygonMode OverridePolygonMode = PolygonMode::Solid;

        void Clear()
        {
            MeshObjects.clear();
            Sprites.clear();
            Texts.clear();
        }
    };

} // namespace Crowny
