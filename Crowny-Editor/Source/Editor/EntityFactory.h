#pragma once

#include "Crowny/Assets/AssetHandle.h"
#include "Crowny/Ecs/Entity.h"
#include "Crowny/Renderer/PrimitiveMeshLibrary.h"
#include "Crowny/Renderer/RenderLight.h"

namespace Crowny
{
    class Mesh;
    class Scene;

    // Creates the Unity-style "GameObject" presets (empty, lights, 3D primitives). Shared by the Hierarchy
    // context menu and any future main-menu entry. The factory only builds entities; callers register undo
    // actions and selection (see HierarchyPanel::CreateEntityFromFactory).
    class EntityFactory
    {
    public:
        static constexpr const char* DefaultEntityName = "New Entity";

        static const char* GetLightName(LightType type);
        static const char* GetPrimitiveName(PrimitiveMeshType type) { return PrimitiveMeshLibrary::GetName(type); }

        // All entities are parented to `parent` when it is valid and belongs to `scene`, otherwise to the scene root.
        static Entity CreateEmpty(const Ref<Scene>& scene, Entity parent, const String& name = DefaultEntityName);
        static Entity CreateLight(const Ref<Scene>& scene, Entity parent, LightType type);
        static Entity CreatePrimitive(const Ref<Scene>& scene, Entity parent, PrimitiveMeshType type);

        // Entity with a MeshRendererComponent referencing `mesh`. Materials are left empty so the renderer's
        // default material applies, matching the viewport mesh drag-and-drop behaviour.
        static Entity CreateMeshEntity(const Ref<Scene>& scene, Entity parent, const String& name, const AssetHandle<Mesh>& mesh);

        // Fills a LightComponent with the preset defaults for the given type (does not touch the transform).
        static void ApplyLightDefaults(struct LightComponent& light, LightType type);
    };
} // namespace Crowny
