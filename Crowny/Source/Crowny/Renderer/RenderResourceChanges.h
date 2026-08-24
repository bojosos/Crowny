#pragma once

#include "Crowny/Assets/AssetHandle.h"

namespace Crowny
{
    class Material;
    class Mesh;

    enum class RenderResourceChangeType : uint8_t
    {
        CreateOrUpdate,
        Destroy
    };

    struct RenderMeshResourceChange
    {
        uint32_t Index = 0;
        uint64_t Version = 0;
        RenderResourceChangeType Type = RenderResourceChangeType::CreateOrUpdate;
        AssetHandle<Mesh> Resource;
    };

    struct RenderMaterialResourceChange
    {
        uint32_t Index = 0;
        uint64_t Version = 0;
        RenderResourceChangeType Type = RenderResourceChangeType::CreateOrUpdate;
        AssetHandle<Material> Resource;
    };
} // namespace Crowny
