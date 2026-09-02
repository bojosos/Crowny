#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Renderer/Mesh.h"

namespace Crowny
{
    // Built-in primitive shapes exposed to the editor ("3D Object" menu) and scripts.
    enum class PrimitiveMeshType : uint8_t
    {
        Cube,
        Sphere,
        Plane,
        Cylinder,
        Cone,
        Capsule,
        Count
    };

    // Owns the shared primitive meshes and registers them with the AssetManager under fixed,
    // well-known UUIDs. Because the UUIDs never change, a MeshRendererComponent that references
    // a primitive serializes the same UUID every time and resolves back to the shared mesh on load
    // without any asset file on disk (LoadFromUUID finds the registered handle, or the placeholder
    // handle created by the scene loader is filled in by EnsureRegistered()).
    class PrimitiveMeshLibrary
    {
    public:
        static constexpr uint32_t GetCount() { return static_cast<uint32_t>(PrimitiveMeshType::Count); }

        static const char* GetName(PrimitiveMeshType type);
        static const UUID& GetUuid(PrimitiveMeshType type);
        static bool TryGetType(const UUID& uuid, PrimitiveMeshType& outType);
        static bool IsPrimitiveMesh(const UUID& uuid) { PrimitiveMeshType type; return TryGetType(uuid, type); }

        // CPU-side geometry for the primitive using the library's canonical dimensions (unit-sized shapes).
        static Ref<MeshData> CreateData(PrimitiveMeshType type);

        // Returns the shared handle for the primitive. The mesh is created (GPU buffers included) on first
        // use when a RenderAPI is available; without one (tests, headless tools) the returned handle is an
        // unloaded AssetManager placeholder that still carries the primitive's UUID so it serializes correctly.
        // Returns an empty handle when no AssetManager is running.
        static AssetHandle<Mesh> GetMesh(PrimitiveMeshType type);

        // Creates and registers every primitive that is not registered yet. Cheap once everything is cached.
        // Call after the RenderAPI and AssetManager are up (and after loading scenes that reference primitives).
        static void EnsureRegistered();

        // Drops the cached handles. Call before the RenderAPI shuts down.
        static void Shutdown();

    private:
        static AssetHandle<Mesh> Register(PrimitiveMeshType type);
    };
} // namespace Crowny
