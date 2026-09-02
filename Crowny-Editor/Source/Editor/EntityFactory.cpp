#include "cwepch.h"

#include "Editor/EntityFactory.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Renderer/Mesh.h"
#include "Crowny/Scene/Scene.h"

#include <glm/gtc/quaternion.hpp>

namespace Crowny
{
    namespace
    {
        Entity ResolveParent(const Ref<Scene>& scene, Entity parent)
        {
            if (parent && parent.GetScene() == scene.get())
                return parent;
            return scene->GetRootEntity();
        }

        Entity CreateChild(const Ref<Scene>& scene, Entity parent, const String& name)
        {
            Entity entity = scene->CreateEntity(name);
            Entity resolvedParent = ResolveParent(scene, parent);
            if (resolvedParent && resolvedParent != entity.GetParent())
                resolvedParent.AddChild(entity);
            return entity;
        }
    } // namespace

    const char* EntityFactory::GetLightName(LightType type)
    {
        switch (type)
        {
        case LightType::Directional:
            return "Directional Light";
        case LightType::Point:
            return "Point Light";
        case LightType::Spot:
            return "Spot Light";
        }
        return "Light";
    }

    void EntityFactory::ApplyLightDefaults(LightComponent& light, LightType type)
    {
        light.Type = type;
        light.Color = glm::vec3(1.0f);
        switch (type)
        {
        case LightType::Directional:
            light.Intensity = 100000.0f; // Lux, roughly overcast-to-clear daylight.
            light.Shadows.Mode = LightShadowMode::Soft;
            break;
        case LightType::Point:
            light.Intensity = 1000.0f; // Lumens.
            light.Range = 10.0f;
            break;
        case LightType::Spot:
            light.Intensity = 1000.0f; // Lumens.
            light.Range = 10.0f;
            light.SpotInnerAngle = glm::radians(25.0f);
            light.SpotOuterAngle = glm::radians(35.0f);
            break;
        }
    }

    Entity EntityFactory::CreateEmpty(const Ref<Scene>& scene, Entity parent, const String& name)
    {
        if (!scene)
            return Entity::Invalid;
        return CreateChild(scene, parent, name);
    }

    Entity EntityFactory::CreateLight(const Ref<Scene>& scene, Entity parent, LightType type)
    {
        if (!scene)
            return Entity::Invalid;
        Entity entity = CreateChild(scene, parent, GetLightName(type));
        ApplyLightDefaults(entity.AddComponent<LightComponent>(), type);
        if (type == LightType::Directional)
        {
            // Unity's default sun orientation: pitched down 50 degrees, yawed -30 degrees.
            entity.GetTransform().SetRotation(glm::quat(glm::radians(glm::vec3(50.0f, -30.0f, 0.0f))));
        }
        return entity;
    }

    Entity EntityFactory::CreateMeshEntity(const Ref<Scene>& scene, Entity parent, const String& name, const AssetHandle<Mesh>& mesh)
    {
        if (!scene)
            return Entity::Invalid;
        Entity entity = CreateChild(scene, parent, name);
        MeshRendererComponent& meshRenderer = entity.AddComponent<MeshRendererComponent>();
        meshRenderer.MeshHandle = mesh;
        return entity;
    }

    Entity EntityFactory::CreatePrimitive(const Ref<Scene>& scene, Entity parent, PrimitiveMeshType type)
    {
        if (!scene || static_cast<uint32_t>(type) >= PrimitiveMeshLibrary::GetCount())
            return Entity::Invalid;
        return CreateMeshEntity(scene, parent, GetPrimitiveName(type), PrimitiveMeshLibrary::GetMesh(type));
    }
} // namespace Crowny
