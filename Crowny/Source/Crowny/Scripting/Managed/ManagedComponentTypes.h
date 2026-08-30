#pragma once

// Managed name, native ECS component, Mono adapter wrapper. This is the single
// built-in component registry used by the shared host ABI and Mono wrapper dispatch.
#define CW_MANAGED_COMPONENT_TYPES(X)                                                                                                                \
    X("Crowny.AnimationComponent", AnimationComponent, ScriptAnimation)                                                                              \
    X("Crowny.Transform", TransformComponent, ScriptTransform)                                                                                       \
    X("Crowny.Camera", CameraComponent, ScriptCamera)                                                                                                \
    X("Crowny.LightComponent", LightComponent, ScriptLight)                                                                                          \
    X("Crowny.AudioSource", AudioSourceComponent, ScriptAudioSource)                                                                                 \
    X("Crowny.AudioListener", AudioListenerComponent, ScriptAudioListener)                                                                           \
    X("Crowny.Rigidbody2D", Rigidbody2DComponent, ScriptRigidbody2D)                                                                                 \
    X("Crowny.Rigidbody3D", Rigidbody3DComponent, ScriptRigidbody3D)                                                                                 \
    X("Crowny.Collider2D", Collider2D, ScriptCollider2D)                                                                                             \
    X("Crowny.CircleCollider2D", CircleCollider2DComponent, ScriptCircleCollider2D)                                                                  \
    X("Crowny.BoxCollider2D", BoxCollider2DComponent, ScriptBoxCollider2D)                                                                           \
    X("Crowny.Collider3D", Collider3D, ScriptCollider3D)                                                                                             \
    X("Crowny.BoxCollider3D", BoxCollider3DComponent, ScriptBoxCollider3D)                                                                           \
    X("Crowny.SphereCollider3D", SphereCollider3DComponent, ScriptSphereCollider3D)                                                                  \
    X("Crowny.CapsuleCollider3D", CapsuleCollider3DComponent, ScriptCapsuleCollider3D)                                                               \
    X("Crowny.SpriteRendererComponent", SpriteRendererComponent, ScriptSpriteRenderer)                                                               \
    X("Crowny.MeshRenderer", MeshRendererComponent, ScriptMeshComponent)                                                                             \
    X("Crowny.Text", TextComponent, ScriptText)
