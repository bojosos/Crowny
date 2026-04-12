#include "cwpch.h"

#include "Crowny/Scene/PrefabSync.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"

namespace Crowny
{
    // Helper to check if any property of a component is overridden
    static bool HasAnyOverride(const PrefabComponent& pc, const String& componentName)
    {
        String prefix = componentName + ".";
        for (const auto& ovr : pc.Overrides)
        {
            if (ovr.compare(0, prefix.length(), prefix) == 0)
                return true;
        }
        return false;
    }

    template <typename T> void PrefabSync::SyncComponent(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        // Default implementation: if no sub-properties are overridden, copy the whole component.
        // This is only called if both have the component.
        // Specialized versions below handle per-property overrides.
        instance.AddOrReplaceComponent<T>(prefab.GetComponent<T>());
    }

    template <> void PrefabSync::SyncComponent<TransformComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<TransformComponent>();
        const auto& pref = prefab.GetComponent<TransformComponent>();

        if (!pc.IsPropertyOverridden("Transform.Position"))
            inst.SetPosition(pref.GetLocalTransform().GetPosition());
        if (!pc.IsPropertyOverridden("Transform.Rotation"))
            inst.SetRotation(pref.GetLocalTransform().GetRotation());
        if (!pc.IsPropertyOverridden("Transform.Scale"))
            inst.SetScale(pref.GetLocalTransform().GetScale());
    }

    template <> void PrefabSync::SyncComponent<CameraComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<CameraComponent>();
        const auto& pref = prefab.GetComponent<CameraComponent>();

        if (!pc.IsPropertyOverridden("Camera.ProjectionType"))
            inst.Camera.SetProjectionType(pref.Camera.GetProjectionType());
        if (!pc.IsPropertyOverridden("Camera.PerspectiveFOV"))
            inst.Camera.SetPerspectiveVerticalFOV(pref.Camera.GetPerspectiveVerticalFOV());
        if (!pc.IsPropertyOverridden("Camera.PerspectiveNear"))
            inst.Camera.SetPerspectiveNearClip(pref.Camera.GetPerspectiveNearClip());
        if (!pc.IsPropertyOverridden("Camera.PerspectiveFar"))
            inst.Camera.SetPerspectiveFarClip(pref.Camera.GetPerspectiveFarClip());
        if (!pc.IsPropertyOverridden("Camera.BackgroundColor"))
            inst.Camera.SetBackgroundColor(pref.Camera.GetBackgroundColor());
    }

    template <> void PrefabSync::SyncComponent<SpriteRendererComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<SpriteRendererComponent>();
        const auto& pref = prefab.GetComponent<SpriteRendererComponent>();

        if (!pc.IsPropertyOverridden("Sprite Renderer.Color"))
            inst.Color = pref.Color;
        if (!pc.IsPropertyOverridden("Sprite Renderer.Texture"))
            inst.Texture = pref.Texture;
    }

    template <> void PrefabSync::SyncComponent<MeshRendererComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<MeshRendererComponent>();
        const auto& pref = prefab.GetComponent<MeshRendererComponent>();

        if (!pc.IsPropertyOverridden("Mesh Filter.Mesh"))
            inst.MeshHandle = pref.MeshHandle;
        if (!pc.IsPropertyOverridden("Mesh Filter.Materials"))
            inst.Materials = pref.Materials;
    }

    template <> void PrefabSync::SyncComponent<TextComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<TextComponent>();
        const auto& pref = prefab.GetComponent<TextComponent>();

        if (!pc.IsPropertyOverridden("Text.Text"))
            inst.Text = pref.Text;
        if (!pc.IsPropertyOverridden("Text.Font"))
            inst.Font = pref.Font;
        if (!pc.IsPropertyOverridden("Text.Color"))
            inst.Color = pref.Color;
        if (!pc.IsPropertyOverridden("Text.Size"))
            inst.Size = pref.Size;
    }

    template <> void PrefabSync::SyncComponent<AudioSourceComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<AudioSourceComponent>();
        const auto& pref = prefab.GetComponent<AudioSourceComponent>();

        if (!pc.IsPropertyOverridden("Audio Source.Volume"))
            inst.SetVolume(pref.GetVolume());
        if (!pc.IsPropertyOverridden("Audio Source.Pitch"))
            inst.SetPitch(pref.GetPitch());
        if (!pc.IsPropertyOverridden("Audio Source.Loop"))
            inst.SetLooping(pref.GetLooping());
        if (!pc.IsPropertyOverridden("Audio Source.PlayOnAwake"))
            inst.SetPlayOnAwake(pref.GetPlayOnAwake());
    }

    template <> void PrefabSync::SyncComponent<Rigidbody2DComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<Rigidbody2DComponent>();
        const auto& pref = prefab.GetComponent<Rigidbody2DComponent>();

        if (!pc.IsPropertyOverridden("Rigidbody 2D.BodyType"))
            inst.SetBodyType(pref.GetBodyType());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.Mass"))
            inst.SetMass(pref.GetMass());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.GravityScale"))
            inst.SetGravityScale(pref.GetGravityScale());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.LinearDrag"))
            inst.SetLinearDrag(pref.GetLinearDrag());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.AngularDrag"))
            inst.SetAngularDrag(pref.GetAngularDrag());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.Constraints"))
            inst.SetConstraints(pref.GetConstraints());
    }

    template <> void PrefabSync::SyncComponent<BoxCollider2DComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<BoxCollider2DComponent>();
        const auto& pref = prefab.GetComponent<BoxCollider2DComponent>();

        if (!pc.IsPropertyOverridden("Box Collider 2D.Offset"))
            inst.SetOffset(pref.GetOffset(), instance);
        if (!pc.IsPropertyOverridden("Box Collider 2D.Size"))
            inst.SetSize(pref.GetSize(), instance);
        if (!pc.IsPropertyOverridden("Box Collider 2D.IsTrigger"))
            inst.SetIsTrigger(pref.IsTrigger());
    }

    template <> void PrefabSync::SyncComponent<CircleCollider2DComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<CircleCollider2DComponent>();
        const auto& pref = prefab.GetComponent<CircleCollider2DComponent>();

        if (!pc.IsPropertyOverridden("Circle Collider 2D.Offset"))
            inst.SetOffset(pref.GetOffset(), instance);
        if (!pc.IsPropertyOverridden("Circle Collider 2D.Radius"))
            inst.SetRadius(pref.GetRadius(), instance);
        if (!pc.IsPropertyOverridden("Circle Collider 2D.IsTrigger"))
            inst.SetIsTrigger(pref.IsTrigger());
    }

    template <typename... Component>
    static void SyncAllComponents(ComponentGroup<Component...>, Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        ([&]() {
            using T = Component;
            if constexpr (std::is_same_v<T, PrefabComponent> || std::is_same_v<T, RelationshipComponent> ||
                          std::is_same_v<T, IDComponent> || std::is_same_v<T, TagComponent>)
                return;

            bool prefabHas = prefab.HasComponent<T>();
            bool instanceHas = instance.HasComponent<T>();

            if (prefabHas && !instanceHas)
            {
                instance.AddComponent<T>(prefab.GetComponent<T>());
            }
            else if (!prefabHas && instanceHas)
            {
                // Component was removed from prefab. We should probably remove it from instance too
                // if it's not overridden (but we don't have "Component Added" overrides yet).
                // instance.RemoveComponent<T>();
            }
            else if (prefabHas && instanceHas)
            {
                PrefabSync::SyncComponent<T>(instance, prefab, pc);
            }
        }(),
         ...);
    }

    void PrefabSync::SyncEntity(Entity instanceEntity, Entity prefabEntity, const PrefabComponent& pc)
    {
        SyncAllComponents(AllComponents{}, instanceEntity, prefabEntity, pc);
    }

} // namespace Crowny
