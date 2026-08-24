#include "cwpch.h"

#include "Crowny/Scene/PrefabSync.h"

#include "Crowny/Ecs/Components.h"
#include "Crowny/Ecs/Entity.h"

namespace Crowny
{
    using namespace Literals;

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

        if (!pc.IsPropertyOverridden("Transform.Position"_hstr))
            inst.SetPosition(pref.GetLocalTransform().GetPosition());
        if (!pc.IsPropertyOverridden("Transform.Rotation"_hstr))
            inst.SetRotation(pref.GetLocalTransform().GetRotation());
        if (!pc.IsPropertyOverridden("Transform.Scale"_hstr))
            inst.SetScale(pref.GetLocalTransform().GetScale());
    }

    template <> void PrefabSync::SyncComponent<CameraComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<CameraComponent>();
        const auto& pref = prefab.GetComponent<CameraComponent>();

        if (!pc.IsPropertyOverridden("Camera.ProjectionType"_hstr))
            inst.Camera.SetProjectionType(pref.Camera.GetProjectionType());
        if (!pc.IsPropertyOverridden("Camera.PerspectiveFOV"_hstr))
            inst.Camera.SetPerspectiveVerticalFOV(pref.Camera.GetPerspectiveVerticalFOV());
        if (!pc.IsPropertyOverridden("Camera.PerspectiveNear"_hstr))
            inst.Camera.SetPerspectiveNearClip(pref.Camera.GetPerspectiveNearClip());
        if (!pc.IsPropertyOverridden("Camera.PerspectiveFar"_hstr))
            inst.Camera.SetPerspectiveFarClip(pref.Camera.GetPerspectiveFarClip());
        if (!pc.IsPropertyOverridden("Camera.BackgroundColor"_hstr))
            inst.Camera.SetBackgroundColor(pref.Camera.GetBackgroundColor());
    }

    template <> void PrefabSync::SyncComponent<SpriteRendererComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<SpriteRendererComponent>();
        const auto& pref = prefab.GetComponent<SpriteRendererComponent>();

        if (!pc.IsPropertyOverridden("Sprite Renderer.Color"_hstr))
            inst.Color = pref.Color;
        if (!pc.IsPropertyOverridden("Sprite Renderer.Texture"_hstr))
            inst.Texture = pref.Texture;
    }

    template <> void PrefabSync::SyncComponent<MeshRendererComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<MeshRendererComponent>();
        const auto& pref = prefab.GetComponent<MeshRendererComponent>();

        if (!pc.IsPropertyOverridden("Mesh Filter.Mesh"_hstr))
            inst.MeshHandle = pref.MeshHandle;
        if (!pc.IsPropertyOverridden("Mesh Filter.Materials"_hstr))
            inst.Materials = pref.Materials;
    }

    template <> void PrefabSync::SyncComponent<TextComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<TextComponent>();
        const auto& pref = prefab.GetComponent<TextComponent>();

        if (!pc.IsPropertyOverridden("Text.Text"_hstr))
            inst.Text = pref.Text;
        if (!pc.IsPropertyOverridden("Text.Font"_hstr))
            inst.Font = pref.Font;
        if (!pc.IsPropertyOverridden("Text.Color"_hstr))
            inst.Color = pref.Color;
        if (!pc.IsPropertyOverridden("Text.Size"_hstr))
            inst.Size = pref.Size;
        if (!pc.IsPropertyOverridden("Text.Auto Size"_hstr))
            inst.AutoSize = pref.AutoSize;
        if (!pc.IsPropertyOverridden("Text.Auto Size Min"_hstr))
            inst.AutoSizeMin = pref.AutoSizeMin;
        if (!pc.IsPropertyOverridden("Text.Auto Size Max"_hstr))
            inst.AutoSizeMax = pref.AutoSizeMax;
        if (!pc.IsPropertyOverridden("Text.Layout Size"_hstr))
            inst.LayoutSize = pref.LayoutSize;
        if (!pc.IsPropertyOverridden("Text.Wrapping"_hstr))
            inst.Wrapping = pref.Wrapping;
        if (!pc.IsPropertyOverridden("Text.Wrap Mode"_hstr))
            inst.WrapMode = pref.WrapMode;
        if (!pc.IsPropertyOverridden("Text.Overflow"_hstr))
            inst.Overflow = pref.Overflow;
        if (!pc.IsPropertyOverridden("Text.Clip To Bounds"_hstr))
            inst.ClipToBounds = pref.ClipToBounds;
        if (!pc.IsPropertyOverridden("Text.Max Lines"_hstr))
            inst.MaxLines = pref.MaxLines;
        if (!pc.IsPropertyOverridden("Text.Horizontal Alignment"_hstr))
            inst.HorizontalAlignment = pref.HorizontalAlignment;
        if (!pc.IsPropertyOverridden("Text.Vertical Alignment"_hstr))
            inst.VerticalAlignment = pref.VerticalAlignment;
        if (!pc.IsPropertyOverridden("Text.Style"_hstr))
            inst.FontStyle = pref.FontStyle;
        if (!pc.IsPropertyOverridden("Text.Outline"_hstr))
            inst.OutlineColor = pref.OutlineColor;
        if (!pc.IsPropertyOverridden("Text.Outline Width"_hstr))
            inst.Thickness = pref.Thickness;
        if (!pc.IsPropertyOverridden("Text.Use Kerning"_hstr))
            inst.UseKerning = pref.UseKerning;
        if (!pc.IsPropertyOverridden("Text.Character Spacing"_hstr))
            inst.CharacterSpacing = pref.CharacterSpacing;
        if (!pc.IsPropertyOverridden("Text.Word Spacing"_hstr))
            inst.WordSpacing = pref.WordSpacing;
        if (!pc.IsPropertyOverridden("Text.Line Spacing"_hstr))
            inst.LineSpacing = pref.LineSpacing;
        if (!pc.IsPropertyOverridden("Text.Paragraph Spacing"_hstr))
            inst.ParagraphSpacing = pref.ParagraphSpacing;
        if (!pc.IsPropertyOverridden("Text.Custom Decoration Color"_hstr))
            inst.UseCustomDecorationColor = pref.UseCustomDecorationColor;
        if (!pc.IsPropertyOverridden("Text.Decoration Color"_hstr))
            inst.DecorationColor = pref.DecorationColor;
        if (!pc.IsPropertyOverridden("Text.Decoration Thickness"_hstr))
            inst.DecorationThickness = pref.DecorationThickness;
        if (!pc.IsPropertyOverridden("Text.Underline Offset"_hstr))
            inst.UnderlineOffset = pref.UnderlineOffset;
        if (!pc.IsPropertyOverridden("Text.Strikethrough Offset"_hstr))
            inst.StrikethroughOffset = pref.StrikethroughOffset;
    }

    template <> void PrefabSync::SyncComponent<AudioSourceComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<AudioSourceComponent>();
        const auto& pref = prefab.GetComponent<AudioSourceComponent>();

        if (!pc.IsPropertyOverridden("Audio Source.Volume"_hstr))
            inst.SetVolume(pref.GetVolume());
        if (!pc.IsPropertyOverridden("Audio Source.Pitch"_hstr))
            inst.SetPitch(pref.GetPitch());
        if (!pc.IsPropertyOverridden("Audio Source.Loop"_hstr))
            inst.SetLooping(pref.GetLooping());
        if (!pc.IsPropertyOverridden("Audio Source.PlayOnAwake"_hstr))
            inst.SetPlayOnAwake(pref.GetPlayOnAwake());
    }

    template <> void PrefabSync::SyncComponent<Rigidbody2DComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<Rigidbody2DComponent>();
        const auto& pref = prefab.GetComponent<Rigidbody2DComponent>();

        if (!pc.IsPropertyOverridden("Rigidbody 2D.BodyType"_hstr))
            inst.SetBodyType(pref.GetBodyType());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.Mass"_hstr))
            inst.SetMass(pref.GetMass());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.GravityScale"_hstr))
            inst.SetGravityScale(pref.GetGravityScale());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.LinearDrag"_hstr))
            inst.SetLinearDrag(pref.GetLinearDrag());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.AngularDrag"_hstr))
            inst.SetAngularDrag(pref.GetAngularDrag());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.Constraints"_hstr))
            inst.SetConstraints(pref.GetConstraints());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.Layer"_hstr))
            inst.SetLayerMask(pref.GetLayerMask(), instance);
        if (!pc.IsPropertyOverridden("Rigidbody 2D.AutoMass"_hstr))
            inst.SetAutoMass(pref.GetAutoMass(), instance);
        if (!pc.IsPropertyOverridden("Rigidbody 2D.CollisionDetection"_hstr))
            inst.SetCollisionDetectionMode(pref.GetCollisionDetectionMode());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.SleepMode"_hstr))
            inst.SetSleepMode(pref.GetSleepMode());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.Interpolation"_hstr))
            inst.SetInterpolationMode(pref.GetInterpolationMode());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.CenterOfMass"_hstr))
            inst.SetCenterOfMass(pref.GetConfiguredCenterOfMass());
        if (!pc.IsPropertyOverridden("Rigidbody 2D.Inertia"_hstr))
            inst.SetInertia(pref.GetConfiguredInertia());
    }

    template <> void PrefabSync::SyncComponent<BoxCollider2DComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<BoxCollider2DComponent>();
        const auto& pref = prefab.GetComponent<BoxCollider2DComponent>();

        if (!pc.IsPropertyOverridden("Box Collider 2D.Offset"_hstr))
            inst.SetOffset(pref.GetOffset(), instance);
        if (!pc.IsPropertyOverridden("Box Collider 2D.Size"_hstr))
            inst.SetSize(pref.GetSize(), instance);
        if (!pc.IsPropertyOverridden("Box Collider 2D.IsTrigger"_hstr))
            inst.SetIsTrigger(pref.IsTrigger());
        if (!pc.IsPropertyOverridden("Box Collider 2D.Material"_hstr))
            inst.SetMaterial(pref.GetMaterial());
    }

    template <> void PrefabSync::SyncComponent<CircleCollider2DComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& inst = instance.GetComponent<CircleCollider2DComponent>();
        const auto& pref = prefab.GetComponent<CircleCollider2DComponent>();

        if (!pc.IsPropertyOverridden("Circle Collider 2D.Offset"_hstr))
            inst.SetOffset(pref.GetOffset(), instance);
        if (!pc.IsPropertyOverridden("Circle Collider 2D.Radius"_hstr))
            inst.SetRadius(pref.GetRadius(), instance);
        if (!pc.IsPropertyOverridden("Circle Collider 2D.IsTrigger"_hstr))
            inst.SetIsTrigger(pref.IsTrigger());
        if (!pc.IsPropertyOverridden("Circle Collider 2D.Material"_hstr))
            inst.SetMaterial(pref.GetMaterial());
    }

    template <typename Collider>
    void SyncCollider3DBase(Collider& instance, const Collider& prefab, Entity instanceEntity, const PrefabComponent& pc, StringView componentName)
    {
        if (!pc.IsPropertyOverridden(componentName, "Offset"))
            instance.SetOffset(prefab.GetOffset(), instanceEntity);
        if (!pc.IsPropertyOverridden(componentName, "Rotation"))
            instance.SetRotation(prefab.GetRotation(), instanceEntity);
        if (!pc.IsPropertyOverridden(componentName, "IsTrigger"))
            instance.SetIsTrigger(prefab.IsTrigger());
        if (!pc.IsPropertyOverridden(componentName, "Material"))
            instance.SetMaterial(prefab.GetMaterial());
        if (!pc.IsPropertyOverridden(componentName, "Filter"))
            instance.SetFilter(prefab.GetFilter(), instanceEntity);
    }

    template <> void PrefabSync::SyncComponent<BoxCollider3DComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& target = instance.GetComponent<BoxCollider3DComponent>();
        const auto& source = prefab.GetComponent<BoxCollider3DComponent>();
        SyncCollider3DBase(target, source, instance, pc, "Box Collider 3D");
        if (!pc.IsPropertyOverridden("Box Collider 3D.Size"_hstr))
            target.SetSize(source.GetSize(), instance);
    }

    template <> void PrefabSync::SyncComponent<SphereCollider3DComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& target = instance.GetComponent<SphereCollider3DComponent>();
        const auto& source = prefab.GetComponent<SphereCollider3DComponent>();
        SyncCollider3DBase(target, source, instance, pc, "Sphere Collider 3D");
        if (!pc.IsPropertyOverridden("Sphere Collider 3D.Radius"_hstr))
            target.SetRadius(source.GetRadius(), instance);
    }

    template <> void PrefabSync::SyncComponent<CapsuleCollider3DComponent>(Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        auto& target = instance.GetComponent<CapsuleCollider3DComponent>();
        const auto& source = prefab.GetComponent<CapsuleCollider3DComponent>();
        SyncCollider3DBase(target, source, instance, pc, "Capsule Collider 3D");
        if (!pc.IsPropertyOverridden("Capsule Collider 3D.Radius"_hstr))
            target.SetRadius(source.GetRadius(), instance);
        if (!pc.IsPropertyOverridden("Capsule Collider 3D.Height"_hstr))
            target.SetHeight(source.GetHeight(), instance);
    }

    template <typename... Component>
    static void SyncAllComponents(ComponentGroup<Component...>, Entity instance, Entity prefab, const PrefabComponent& pc)
    {
        (
          [&]() {
              using T = Component;
              if constexpr (std::is_same_v<T, PrefabComponent> || std::is_same_v<T, RelationshipComponent> || std::is_same_v<T, IDComponent> ||
                            std::is_same_v<T, TagComponent>)
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
