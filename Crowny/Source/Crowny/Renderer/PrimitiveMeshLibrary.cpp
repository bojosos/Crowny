#include "cwpch.h"

#include "Crowny/Renderer/PrimitiveMeshLibrary.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/RenderAPI/RenderAPI.h"
#include "Crowny/Renderer/MeshFactory.h"

namespace Crowny
{
    namespace
    {
        constexpr uint32_t PRIMITIVE_COUNT = static_cast<uint32_t>(PrimitiveMeshType::Count);

        struct PrimitiveInfo
        {
            const char* Name;
            UUID Uuid;
        };

        // Fixed identifiers; never change these or existing scenes lose their primitive references.
        const Array<PrimitiveInfo, PRIMITIVE_COUNT>& GetInfos()
        {
            static const Array<PrimitiveInfo, PRIMITIVE_COUNT> infos = { {
              { "Cube", UUID(0x6b75d1a0u, 0x4c1e4a10u, 0x9f2e0001u, 0x43726f77u) },
              { "Sphere", UUID(0x6b75d1a0u, 0x4c1e4a10u, 0x9f2e0002u, 0x43726f77u) },
              { "Plane", UUID(0x6b75d1a0u, 0x4c1e4a10u, 0x9f2e0003u, 0x43726f77u) },
              { "Cylinder", UUID(0x6b75d1a0u, 0x4c1e4a10u, 0x9f2e0004u, 0x43726f77u) },
              { "Cone", UUID(0x6b75d1a0u, 0x4c1e4a10u, 0x9f2e0005u, 0x43726f77u) },
              { "Capsule", UUID(0x6b75d1a0u, 0x4c1e4a10u, 0x9f2e0006u, 0x43726f77u) },
            } };
            return infos;
        }

        Array<AssetHandle<Mesh>, PRIMITIVE_COUNT>& GetCache()
        {
            static Array<AssetHandle<Mesh>, PRIMITIVE_COUNT> cache;
            return cache;
        }

        bool IsValidType(PrimitiveMeshType type) { return static_cast<uint32_t>(type) < PRIMITIVE_COUNT; }
    } // namespace

    const char* PrimitiveMeshLibrary::GetName(PrimitiveMeshType type)
    {
        return IsValidType(type) ? GetInfos()[static_cast<size_t>(type)].Name : "Unknown";
    }

    const UUID& PrimitiveMeshLibrary::GetUuid(PrimitiveMeshType type)
    {
        return IsValidType(type) ? GetInfos()[static_cast<size_t>(type)].Uuid : UUID::EMPTY;
    }

    bool PrimitiveMeshLibrary::TryGetType(const UUID& uuid, PrimitiveMeshType& outType)
    {
        if (uuid == UUID::EMPTY)
            return false;
        const auto& infos = GetInfos();
        for (uint32_t index = 0; index < PRIMITIVE_COUNT; index++)
        {
            if (infos[index].Uuid == uuid)
            {
                outType = static_cast<PrimitiveMeshType>(index);
                return true;
            }
        }
        return false;
    }

    Ref<MeshData> PrimitiveMeshLibrary::CreateData(PrimitiveMeshType type)
    {
        switch (type)
        {
        case PrimitiveMeshType::Cube:
            return MeshFactory::CreateCubeData(1.0f);
        case PrimitiveMeshType::Sphere:
            return MeshFactory::CreateSphereData(0.5f);
        case PrimitiveMeshType::Plane:
            return MeshFactory::CreatePlaneData(10.0f, 10.0f);
        case PrimitiveMeshType::Cylinder:
            return MeshFactory::CreateCylinderData(0.5f, 2.0f);
        case PrimitiveMeshType::Cone:
            return MeshFactory::CreateConeData(0.5f, 1.0f);
        case PrimitiveMeshType::Capsule:
            return MeshFactory::CreateCapsuleData(0.5f, 2.0f);
        case PrimitiveMeshType::Count:
            break;
        }
        return nullptr;
    }

    AssetHandle<Mesh> PrimitiveMeshLibrary::Register(PrimitiveMeshType type)
    {
        const UUID& uuid = GetUuid(type);
        AssetManager* assetManager = AssetManager::TryGet();
        if (assetManager == nullptr)
            return {}; // Handles are minted by the AssetManager; without one there is nothing to reference.

        // Reuse whatever the AssetManager already knows about (a previous registration that is still alive,
        // or an unloaded placeholder created while deserializing a scene before the library was registered).
        AssetHandle<Asset> existing = assetManager->GetAssetHandle(uuid);
        if (existing.IsLoaded())
            return static_asset_cast<Mesh>(existing);

        if (!RenderAPI::IsStartedUp())
            return static_asset_cast<Mesh>(existing);

        Ref<MeshData> data = CreateData(type);
        if (data == nullptr)
            return static_asset_cast<Mesh>(existing);

        MeshDesc desc;
        desc.Data = data;
        desc.Usage = MeshUsage::Static;
        desc.SubMeshes.emplace_back(0, data->GetIndexCount(), DrawMode::TRIANGLE_LIST);
        Ref<Mesh> mesh = Mesh::Create(desc);
        if (mesh == nullptr)
            return static_asset_cast<Mesh>(existing);
        mesh->SetName(GetName(type));

        // Fills the existing placeholder (if any) so components loaded earlier start rendering.
        return static_asset_cast<Mesh>(assetManager->CreateAssetHandle(mesh, uuid));
    }

    AssetHandle<Mesh> PrimitiveMeshLibrary::GetMesh(PrimitiveMeshType type)
    {
        if (!IsValidType(type))
            return {};

        AssetHandle<Mesh>& cached = GetCache()[static_cast<size_t>(type)];
        // A cached entry is only trusted while the AssetManager still tracks it (tests recreate managers).
        if (cached.IsLoaded() && AssetManager::TryGet() != nullptr && AssetManager::TryGet()->GetAssetHandle(cached.GetUUID()).IsLoaded())
            return cached;

        cached = Register(type);
        return cached;
    }

    void PrimitiveMeshLibrary::EnsureRegistered()
    {
        if (AssetManager::TryGet() == nullptr || !RenderAPI::IsStartedUp())
            return;
        for (uint32_t index = 0; index < PRIMITIVE_COUNT; index++)
        {
            const AssetHandle<Mesh>& cached = GetCache()[index];
            if (!cached.IsLoaded())
                GetMesh(static_cast<PrimitiveMeshType>(index));
        }
    }

    void PrimitiveMeshLibrary::Shutdown()
    {
        for (AssetHandle<Mesh>& handle : GetCache())
            handle = {};
    }
} // namespace Crowny
