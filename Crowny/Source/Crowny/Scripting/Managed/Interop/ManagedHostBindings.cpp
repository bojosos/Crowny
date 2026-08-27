#include "cwpch.h"

#include "Crowny/Scripting/Managed/Interop/ManagedHostBindings.h"

#include "Crowny/Assets/AssetManager.h"
#include "Crowny/Common/Time.h"
#include "Crowny/Ecs/Components.h"
#include "Crowny/Input/Input.h"
#include "Crowny/Physics/Physics2D.h"
#include "Crowny/Scene/SceneManager.h"

#include <cstring>

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Crowny
{
    namespace
    {
        UUID FromAbiUuid(const cw_managed_uuid& uuid)
        {
            const auto word = [&](uint32_t offset) {
                return static_cast<uint32_t>(uuid.bytes[offset]) << 24u | static_cast<uint32_t>(uuid.bytes[offset + 1]) << 16u |
                       static_cast<uint32_t>(uuid.bytes[offset + 2]) << 8u | static_cast<uint32_t>(uuid.bytes[offset + 3]);
            };
            return UUID(word(0), word(4), word(8), word(12));
        }

        cw_managed_uuid ToAbiUuid(const UUID& uuid)
        {
            cw_managed_uuid result{};
            const String text = uuid.ToString();
            uint32_t output = 0;
            uint8_t high = 0;
            bool haveHigh = false;
            for (const char character : text)
            {
                if (character == '-')
                    continue;
                const uint8_t value = character >= '0' && character <= '9' ? static_cast<uint8_t>(character - '0')
                                      : character >= 'a' && character <= 'f' ? static_cast<uint8_t>(character - 'a' + 10)
                                                                            : static_cast<uint8_t>(character - 'A' + 10);
                if (!haveHigh)
                {
                    high = value;
                    haveHigh = true;
                }
                else if (output < 16)
                {
                    result.bytes[output++] = static_cast<uint8_t>((high << 4u) | value);
                    haveHigh = false;
                }
            }
            return result;
        }

        Entity ResolveEntity(const cw_managed_uuid& entityId)
        {
            SceneManager* manager = SceneManager::TryGet();
            const Ref<Scene> scene = manager != nullptr ? manager->GetActiveScene() : nullptr;
            return scene != nullptr ? scene->TryGetEntityFromUuid(FromAbiUuid(entityId)) : Entity();
        }

        template <typename Callback> cw_managed_status Execute(void* context, Callback&& callback)
        {
            if (context == nullptr)
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;
            try
            {
                return callback();
            }
            catch (...)
            {
                return CW_MANAGED_STATUS_MANAGED_EXCEPTION;
            }
        }

        String Decode(cw_managed_string_view value)
        {
            return value.data == nullptr ? String() : String(reinterpret_cast<const char*>(value.data), value.length);
        }

        bool HasComponent(Entity entity, StringView name)
        {
            if (name == "Crowny.Transform")
                return entity.HasComponent<TransformComponent>();
#define CW_HAS_COMPONENT(managedName, nativeType)                                                                                    \
    if (name == managedName)                                                                                                         \
        return entity.HasComponent<nativeType>()
            CW_HAS_COMPONENT("Crowny.AudioListener", AudioListenerComponent);
            CW_HAS_COMPONENT("Crowny.AudioSource", AudioSourceComponent);
            CW_HAS_COMPONENT("Crowny.Camera", CameraComponent);
            CW_HAS_COMPONENT("Crowny.LightComponent", LightComponent);
            CW_HAS_COMPONENT("Crowny.Text", TextComponent);
            CW_HAS_COMPONENT("Crowny.SpriteRendererComponent", SpriteRendererComponent);
            CW_HAS_COMPONENT("Crowny.MeshRenderer", MeshRendererComponent);
            CW_HAS_COMPONENT("Crowny.Rigidbody2D", Rigidbody2DComponent);
            CW_HAS_COMPONENT("Crowny.BoxCollider2D", BoxCollider2DComponent);
            CW_HAS_COMPONENT("Crowny.CircleCollider2D", CircleCollider2DComponent);
            CW_HAS_COMPONENT("Crowny.Rigidbody3D", Rigidbody3DComponent);
            CW_HAS_COMPONENT("Crowny.BoxCollider3D", BoxCollider3DComponent);
            CW_HAS_COMPONENT("Crowny.SphereCollider3D", SphereCollider3DComponent);
            CW_HAS_COMPONENT("Crowny.CapsuleCollider3D", CapsuleCollider3DComponent);
#undef CW_HAS_COMPONENT
            return false;
        }

        bool AddComponent(Entity entity, StringView name)
        {
            if (name == "Crowny.Transform")
                return entity.HasComponent<TransformComponent>();
#define CW_ADD_COMPONENT(managedName, nativeType)                                                                                    \
    if (name == managedName)                                                                                                         \
    {                                                                                                                                \
        entity.AddOrGetComponent<nativeType>();                                                                                      \
        return true;                                                                                                                 \
    }
            CW_ADD_COMPONENT("Crowny.AudioListener", AudioListenerComponent)
            CW_ADD_COMPONENT("Crowny.AudioSource", AudioSourceComponent)
            CW_ADD_COMPONENT("Crowny.Camera", CameraComponent)
            CW_ADD_COMPONENT("Crowny.LightComponent", LightComponent)
            CW_ADD_COMPONENT("Crowny.Text", TextComponent)
            CW_ADD_COMPONENT("Crowny.SpriteRendererComponent", SpriteRendererComponent)
            CW_ADD_COMPONENT("Crowny.MeshRenderer", MeshRendererComponent)
            CW_ADD_COMPONENT("Crowny.Rigidbody2D", Rigidbody2DComponent)
            CW_ADD_COMPONENT("Crowny.BoxCollider2D", BoxCollider2DComponent)
            CW_ADD_COMPONENT("Crowny.CircleCollider2D", CircleCollider2DComponent)
            CW_ADD_COMPONENT("Crowny.Rigidbody3D", Rigidbody3DComponent)
            CW_ADD_COMPONENT("Crowny.BoxCollider3D", BoxCollider3DComponent)
            CW_ADD_COMPONENT("Crowny.SphereCollider3D", SphereCollider3DComponent)
            CW_ADD_COMPONENT("Crowny.CapsuleCollider3D", CapsuleCollider3DComponent)
#undef CW_ADD_COMPONENT
            return false;
        }

        bool RemoveComponent(Entity entity, StringView name)
        {
            if (name == "Crowny.Transform")
                return false;
#define CW_REMOVE_COMPONENT(managedName, nativeType)                                                                                 \
    if (name == managedName)                                                                                                         \
    {                                                                                                                                \
        entity.RemoveComponentIfExists<nativeType>();                                                                                \
        return true;                                                                                                                 \
    }
            CW_REMOVE_COMPONENT("Crowny.AudioListener", AudioListenerComponent)
            CW_REMOVE_COMPONENT("Crowny.AudioSource", AudioSourceComponent)
            CW_REMOVE_COMPONENT("Crowny.Camera", CameraComponent)
            CW_REMOVE_COMPONENT("Crowny.LightComponent", LightComponent)
            CW_REMOVE_COMPONENT("Crowny.Text", TextComponent)
            CW_REMOVE_COMPONENT("Crowny.SpriteRendererComponent", SpriteRendererComponent)
            CW_REMOVE_COMPONENT("Crowny.MeshRenderer", MeshRendererComponent)
            CW_REMOVE_COMPONENT("Crowny.Rigidbody2D", Rigidbody2DComponent)
            CW_REMOVE_COMPONENT("Crowny.BoxCollider2D", BoxCollider2DComponent)
            CW_REMOVE_COMPONENT("Crowny.CircleCollider2D", CircleCollider2DComponent)
            CW_REMOVE_COMPONENT("Crowny.Rigidbody3D", Rigidbody3DComponent)
            CW_REMOVE_COMPONENT("Crowny.BoxCollider3D", BoxCollider3DComponent)
            CW_REMOVE_COMPONENT("Crowny.SphereCollider3D", SphereCollider3DComponent)
            CW_REMOVE_COMPONENT("Crowny.CapsuleCollider3D", CapsuleCollider3DComponent)
#undef CW_REMOVE_COMPONENT
            return false;
        }

        cw_managed_status CW_MANAGED_CALL EntityHasComponent(void* context, cw_managed_uuid entityId,
                                                              cw_managed_string_view typeName, uint8_t* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr || (typeName.data == nullptr && typeName.length != 0))
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                *result = HasComponent(entity, Decode(typeName)) ? 1 : 0;
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL EntityAddComponent(void* context, cw_managed_uuid entityId,
                                                              cw_managed_string_view typeName)
        {
            return Execute(context, [&]() {
                if (typeName.data == nullptr && typeName.length != 0)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                return AddComponent(entity, Decode(typeName)) ? CW_MANAGED_STATUS_OK : CW_MANAGED_STATUS_INVALID_ARGUMENT;
            });
        }

        cw_managed_status CW_MANAGED_CALL EntityRemoveComponent(void* context, cw_managed_uuid entityId,
                                                                 cw_managed_string_view typeName)
        {
            return Execute(context, [&]() {
                if (typeName.data == nullptr && typeName.length != 0)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const Entity entity = ResolveEntity(entityId);
                if (!entity)
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                return RemoveComponent(entity, Decode(typeName)) ? CW_MANAGED_STATUS_OK : CW_MANAGED_STATUS_INVALID_ARGUMENT;
            });
        }

#define CW_TRANSFORM_GET_VEC3(functionName, expression)                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_vec3* result)                 \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity)                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const glm::vec3 value = expression;                                                                                      \
            *result = { value.x, value.y, value.z };                                                                                 \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_TRANSFORM_SET_VEC3(functionName, statement)                                                                               \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, const cw_managed_vec3* value)            \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                 \
            if (!entity)                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (value == nullptr)                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const glm::vec3 vector(value->x, value->y, value->z);                                                                    \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_TRANSFORM_GET_VEC3(TransformGetPosition, entity.GetWorldPosition())
        CW_TRANSFORM_SET_VEC3(TransformSetPosition, entity.SetWorldPosition(vector))
        CW_TRANSFORM_GET_VEC3(TransformGetLocalPosition, entity.GetLocalPosition())
        CW_TRANSFORM_SET_VEC3(TransformSetLocalPosition, entity.SetPosition(vector))
        CW_TRANSFORM_GET_VEC3(TransformGetScale, entity.GetWorldScale())
        CW_TRANSFORM_SET_VEC3(TransformSetScale, entity.SetWorldScale(vector))
        CW_TRANSFORM_GET_VEC3(TransformGetLocalScale, entity.GetLocalScale())
        CW_TRANSFORM_SET_VEC3(TransformSetLocalScale, entity.SetScale(vector))
        CW_TRANSFORM_GET_VEC3(TransformGetEulerAngles, glm::degrees(glm::eulerAngles(entity.GetWorldRotation())))
        CW_TRANSFORM_SET_VEC3(TransformSetEulerAngles, entity.SetWorldRotation(glm::quat(glm::radians(vector))))
        CW_TRANSFORM_GET_VEC3(TransformGetLocalEulerAngles, glm::degrees(glm::eulerAngles(entity.GetLocalRotation())))
        CW_TRANSFORM_SET_VEC3(TransformSetLocalEulerAngles, entity.SetRotation(glm::quat(glm::radians(vector))))
#undef CW_TRANSFORM_SET_VEC3
#undef CW_TRANSFORM_GET_VEC3

#define CW_TRANSFORM_GET_QUAT(functionName, expression)                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_quat* result)                 \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity)                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const glm::quat value = expression;                                                                                      \
            *result = { value.x, value.y, value.z, value.w };                                                                        \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_TRANSFORM_SET_QUAT(functionName, statement)                                                                               \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, const cw_managed_quat* value)            \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                 \
            if (!entity)                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (value == nullptr)                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const glm::quat rotation(value->w, value->x, value->y, value->z);                                                        \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_TRANSFORM_GET_QUAT(TransformGetRotation, entity.GetWorldRotation())
        CW_TRANSFORM_SET_QUAT(TransformSetRotation, entity.SetWorldRotation(rotation))
        CW_TRANSFORM_GET_QUAT(TransformGetLocalRotation, entity.GetLocalRotation())
        CW_TRANSFORM_SET_QUAT(TransformSetLocalRotation, entity.SetRotation(rotation))
#undef CW_TRANSFORM_SET_QUAT
#undef CW_TRANSFORM_GET_QUAT

#define CW_TRANSFORM_GET_MATRIX(functionName, expression)                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_mat4* result)                 \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity)                                                                                                             \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            const glm::mat4 value = expression;                                                                                      \
            std::memcpy(result->values, glm::value_ptr(value), sizeof(value));                                                       \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_TRANSFORM_GET_MATRIX(TransformGetLocalToWorldMatrix, entity.GetWorldMatrix())
        CW_TRANSFORM_GET_MATRIX(TransformGetWorldToLocalMatrix, glm::inverse(entity.GetWorldMatrix()))
#undef CW_TRANSFORM_GET_MATRIX

#define CW_INPUT_BUTTON(functionName, expression)                                                                                    \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, uint32_t code, uint8_t* result)                                   \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            *result = expression ? 1 : 0;                                                                                            \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_INPUT_BUTTON(InputGetKey, Input::IsKeyPressed(static_cast<KeyCode>(code)))
        CW_INPUT_BUTTON(InputGetKeyDown, Input::IsKeyDown(static_cast<KeyCode>(code)))
        CW_INPUT_BUTTON(InputGetKeyUp, Input::IsKeyUp(static_cast<KeyCode>(code)))
        CW_INPUT_BUTTON(InputGetMouseButton, Input::IsMouseButtonPressed(static_cast<MouseCode>(code)))
        CW_INPUT_BUTTON(InputGetMouseButtonDown, Input::IsMouseButtonDown(static_cast<MouseCode>(code)))
        CW_INPUT_BUTTON(InputGetMouseButtonUp, Input::IsMouseButtonUp(static_cast<MouseCode>(code)))
#undef CW_INPUT_BUTTON

#define CW_GLOBAL_FLOAT(functionName, expression)                                                                                    \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, float* result)                                                     \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            *result = expression;                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_GLOBAL_FLOAT(InputGetMouseScrollX, Input::GetMouseScrollX())
        CW_GLOBAL_FLOAT(InputGetMouseScrollY, Input::GetMouseScrollY())
        CW_GLOBAL_FLOAT(TimeGetTime, Time::GetTime())
        CW_GLOBAL_FLOAT(TimeGetFixedDeltaTime, Time::GetFixedDeltaTime())
        CW_GLOBAL_FLOAT(TimeGetSmoothDeltaTime, Time::GetSmoothDeltaTime())
        CW_GLOBAL_FLOAT(TimeGetRealtimeSinceStartup, Time::GetRealtimeSinceStartup())
        CW_GLOBAL_FLOAT(TimeGetFrameCount, Time::GetFrameCount())
#undef CW_GLOBAL_FLOAT

        cw_managed_status CW_MANAGED_CALL InputGetMousePosition(void* context, cw_managed_vec2* result)
        {
            return Execute(context, [&]() {
                if (result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                const glm::vec2 value = Input::GetMousePosition();
                *result = { value.x, value.y };
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_RIGIDBODY_GET(functionName, resultType, expression)                                                                       \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                     \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();                                                          \
            Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;                                          \
            *result = expression;                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_RIGIDBODY_SET(functionName, valueType, statement)                                                                         \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                        \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                 \
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();                                                          \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_RIGIDBODY_GET(Rigidbody2DGetMass, float, physics != nullptr ? physics->GetMass(entity) : rigidbody.GetMass())
        CW_RIGIDBODY_SET(Rigidbody2DSetMass, float, if (rigidbody.GetAutoMass()) return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                         rigidbody.SetMass(value))
        CW_RIGIDBODY_GET(Rigidbody2DGetBodyType, int32_t, static_cast<int32_t>(rigidbody.GetBodyType()))
        CW_RIGIDBODY_SET(Rigidbody2DSetBodyType, int32_t, rigidbody.SetBodyType(static_cast<RigidbodyBodyType>(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetSleepMode, int32_t, static_cast<int32_t>(rigidbody.GetSleepMode()))
        CW_RIGIDBODY_SET(Rigidbody2DSetSleepMode, int32_t, rigidbody.SetSleepMode(static_cast<RigidbodySleepMode>(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetCollisionDetectionMode, int32_t,
                         static_cast<int32_t>(rigidbody.GetCollisionDetectionMode()))
        CW_RIGIDBODY_SET(Rigidbody2DSetCollisionDetectionMode, int32_t,
                         rigidbody.SetCollisionDetectionMode(static_cast<CollisionDetectionMode2D>(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetInterpolation, int32_t, static_cast<int32_t>(rigidbody.GetInterpolationMode()))
        CW_RIGIDBODY_SET(Rigidbody2DSetInterpolation, int32_t,
                         rigidbody.SetInterpolationMode(static_cast<RigidbodyInterpolation>(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetAutoMass, uint8_t, rigidbody.GetAutoMass() ? 1 : 0)
        CW_RIGIDBODY_SET(Rigidbody2DSetAutoMass, uint8_t, rigidbody.SetAutoMass(value != 0, entity))
        CW_RIGIDBODY_GET(Rigidbody2DGetLayer, int32_t, static_cast<int32_t>(rigidbody.GetLayerMask()))
        CW_RIGIDBODY_SET(Rigidbody2DSetLayer, int32_t,
                         if (value < 0 || value >= static_cast<int32_t>(Physics2DLayerCount)) return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                         rigidbody.SetLayerMask(static_cast<uint32_t>(value), entity))
        CW_RIGIDBODY_GET(Rigidbody2DGetLinearDrag, float, rigidbody.GetLinearDrag())
        CW_RIGIDBODY_SET(Rigidbody2DSetLinearDrag, float, rigidbody.SetLinearDrag(value))
        CW_RIGIDBODY_GET(Rigidbody2DGetAngularDrag, float, rigidbody.GetAngularDrag())
        CW_RIGIDBODY_SET(Rigidbody2DSetAngularDrag, float, rigidbody.SetAngularDrag(value))
        CW_RIGIDBODY_GET(Rigidbody2DGetGravityScale, float, rigidbody.GetGravityScale())
        CW_RIGIDBODY_SET(Rigidbody2DSetGravityScale, float, rigidbody.SetGravityScale(value))
        CW_RIGIDBODY_GET(Rigidbody2DGetInertia, float, physics != nullptr ? physics->GetInertia(entity) : rigidbody.GetInertia())
        CW_RIGIDBODY_SET(Rigidbody2DSetInertia, float, rigidbody.SetInertia(value))
        CW_RIGIDBODY_GET(Rigidbody2DGetConstraints, uint32_t, static_cast<uint32_t>(rigidbody.GetConstraints()))
        CW_RIGIDBODY_SET(Rigidbody2DSetConstraints, uint32_t, rigidbody.SetConstraints(Rigidbody2DConstraints(value)))
        CW_RIGIDBODY_GET(Rigidbody2DGetRotation, float, physics != nullptr ? physics->GetRotation(entity) : 0.0f)
        CW_RIGIDBODY_GET(Rigidbody2DGetAngularVelocity, float, physics != nullptr ? physics->GetAngularVelocity(entity) : 0.0f)
        CW_RIGIDBODY_SET(Rigidbody2DSetAngularVelocity, float,
                         if (Physics2D::IsStartedUp()) Physics2D::TryGet()->SetAngularVelocity(entity, value))
        CW_RIGIDBODY_GET(Rigidbody2DGetAwake, uint8_t, physics != nullptr && physics->IsBodyAwake(entity) ? 1 : 0)
        CW_RIGIDBODY_SET(Rigidbody2DSetAwake, uint8_t,
                         if (Physics2D::IsStartedUp()) Physics2D::TryGet()->SetBodyAwake(entity, value != 0))
#undef CW_RIGIDBODY_SET
#undef CW_RIGIDBODY_GET

#define CW_RIGIDBODY_GET_VEC2(functionName, expression)                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, cw_managed_vec2* result)                \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();                                                          \
            Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;                                          \
            const glm::vec2 value = expression;                                                                                      \
            *result = { value.x, value.y };                                                                                          \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_RIGIDBODY_SET_VEC2(functionName, statement)                                                                               \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, const cw_managed_vec2* value)           \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            Entity entity = ResolveEntity(entityId);                                                                                 \
            if (!entity || !entity.HasComponent<Rigidbody2DComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (value == nullptr)                                                                                                    \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            auto& rigidbody = entity.GetComponent<Rigidbody2DComponent>();                                                          \
            const glm::vec2 vector(value->x, value->y);                                                                              \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_RIGIDBODY_GET_VEC2(Rigidbody2DGetCenterOfMass,
                              physics != nullptr ? physics->GetCenterOfMass(entity) : rigidbody.GetCenterOfMass())
        CW_RIGIDBODY_SET_VEC2(Rigidbody2DSetCenterOfMass, rigidbody.SetCenterOfMass(vector))
        CW_RIGIDBODY_GET_VEC2(Rigidbody2DGetPosition,
                              physics != nullptr ? physics->GetPosition(entity) : glm::vec2(entity.GetWorldPosition()))
        CW_RIGIDBODY_GET_VEC2(Rigidbody2DGetLinearVelocity,
                              physics != nullptr ? physics->GetLinearVelocity(entity) : glm::vec2(0.0f))
        CW_RIGIDBODY_SET_VEC2(Rigidbody2DSetLinearVelocity,
                              if (Physics2D::IsStartedUp()) Physics2D::TryGet()->SetLinearVelocity(entity, vector))
#undef CW_RIGIDBODY_SET_VEC2
#undef CW_RIGIDBODY_GET_VEC2

        cw_managed_status CW_MANAGED_CALL Rigidbody2DAddForce(void* context, cw_managed_uuid entityId,
                                                              const cw_managed_vec2* force, int32_t mode)
        {
            return Execute(context, [&]() {
                const Entity entity = ResolveEntity(entityId);
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (force == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                physics->AddForce(entity, glm::vec2(force->x, force->y), static_cast<ForceMode>(mode));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Rigidbody2DAddForceAtPosition(void* context, cw_managed_uuid entityId,
                                                                        const cw_managed_vec2* force,
                                                                        const cw_managed_vec2* position, int32_t mode)
        {
            return Execute(context, [&]() {
                const Entity entity = ResolveEntity(entityId);
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (force == nullptr || position == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                physics->AddForceAt(entity, glm::vec2(force->x, force->y), glm::vec2(position->x, position->y),
                                    static_cast<ForceMode>(mode));
                return CW_MANAGED_STATUS_OK;
            });
        }

        cw_managed_status CW_MANAGED_CALL Rigidbody2DAddTorque(void* context, cw_managed_uuid entityId, float torque,
                                                               int32_t mode)
        {
            return Execute(context, [&]() {
                const Entity entity = ResolveEntity(entityId);
                Physics2D* physics = Physics2D::IsStartedUp() ? Physics2D::TryGet() : nullptr;
                if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
                    return CW_MANAGED_STATUS_STALE_HANDLE;
                if (physics == nullptr)
                    return CW_MANAGED_STATUS_NOT_INITIALIZED;
                physics->AddTorque(entity, torque, static_cast<ForceMode>(mode));
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_AUDIO_GET(functionName, resultType, expression)                                                                            \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, resultType* result)                     \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<AudioSourceComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            if (result == nullptr)                                                                                                   \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            auto& source = entity.GetComponent<AudioSourceComponent>();                                                             \
            *result = expression;                                                                                                    \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
#define CW_AUDIO_SET(functionName, valueType, statement)                                                                              \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId, valueType value)                        \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<AudioSourceComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            auto& source = entity.GetComponent<AudioSourceComponent>();                                                             \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_AUDIO_GET(AudioSourceGetVolume, float, source.GetVolume())
        CW_AUDIO_SET(AudioSourceSetVolume, float, source.SetVolume(value))
        CW_AUDIO_GET(AudioSourceGetPitch, float, source.GetPitch())
        CW_AUDIO_SET(AudioSourceSetPitch, float, source.SetPitch(value))
        CW_AUDIO_GET(AudioSourceGetMinDistance, float, source.GetMinDistance())
        CW_AUDIO_SET(AudioSourceSetMinDistance, float, source.SetMinDistance(value))
        CW_AUDIO_GET(AudioSourceGetMaxDistance, float, source.GetMaxDistance())
        CW_AUDIO_SET(AudioSourceSetMaxDistance, float, source.SetMaxDistance(value))
        CW_AUDIO_GET(AudioSourceGetLoop, uint8_t, source.GetLooping() ? 1 : 0)
        CW_AUDIO_SET(AudioSourceSetLoop, uint8_t, source.SetLooping(value != 0))
        CW_AUDIO_GET(AudioSourceGetMuted, uint8_t, source.GetIsMuted() ? 1 : 0)
        CW_AUDIO_SET(AudioSourceSetMuted, uint8_t, source.SetIsMuted(value != 0))
        CW_AUDIO_GET(AudioSourceGetPlayOnAwake, uint8_t, source.GetPlayOnAwake() ? 1 : 0)
        CW_AUDIO_SET(AudioSourceSetPlayOnAwake, uint8_t, source.SetPlayOnAwake(value != 0))
        CW_AUDIO_GET(AudioSourceGetTime, float, source.GetTime())
        CW_AUDIO_SET(AudioSourceSetTime, float, source.SetTime(value))
        CW_AUDIO_GET(AudioSourceGetClip, cw_managed_uuid, ToAbiUuid(source.GetClip().GetUUID()))
        CW_AUDIO_SET(AudioSourceSetClip, cw_managed_uuid,
                     if (const UUID uuid = FromAbiUuid(value); uuid.Empty()) source.SetClip({});
                     else if (AssetManager::IsStartedUp()) source.SetClip(AssetManager::TryGet()->LoadFromUUID<AudioClip>(uuid));
                     else return CW_MANAGED_STATUS_NOT_INITIALIZED)
        CW_AUDIO_GET(AudioSourceGetState, int32_t, static_cast<int32_t>(source.GetState()))
#undef CW_AUDIO_SET
#undef CW_AUDIO_GET

#define CW_AUDIO_ACTION(functionName, statement)                                                                                      \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, cw_managed_uuid entityId)                                         \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            const Entity entity = ResolveEntity(entityId);                                                                          \
            if (!entity || !entity.HasComponent<AudioSourceComponent>())                                                            \
                return CW_MANAGED_STATUS_STALE_HANDLE;                                                                               \
            auto& source = entity.GetComponent<AudioSourceComponent>();                                                             \
            statement;                                                                                                               \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_AUDIO_ACTION(AudioSourcePlay, source.Play())
        CW_AUDIO_ACTION(AudioSourcePause, source.Pause())
        CW_AUDIO_ACTION(AudioSourceStop, source.Stop())
#undef CW_AUDIO_ACTION

        glm::mat4 ToMatrix(const cw_managed_mat4& value)
        {
            glm::mat4 result(1.0f);
            std::memcpy(glm::value_ptr(result), value.values, sizeof(result));
            return result;
        }

        void WriteMatrix(const glm::mat4& value, cw_managed_mat4& result)
        {
            std::memcpy(result.values, glm::value_ptr(value), sizeof(value));
        }

        cw_managed_status CW_MANAGED_CALL MathMatrixDeterminant(void* context, const cw_managed_mat4* matrix, float* result)
        {
            return Execute(context, [&]() {
                if (matrix == nullptr || result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                *result = glm::determinant(ToMatrix(*matrix));
                return CW_MANAGED_STATUS_OK;
            });
        }

#define CW_MATRIX_OPERATION(functionName, expression)                                                                                \
    cw_managed_status CW_MANAGED_CALL functionName(void* context, const cw_managed_mat4* matrix, cw_managed_mat4* result)           \
    {                                                                                                                                \
        return Execute(context, [&]() {                                                                                              \
            if (matrix == nullptr || result == nullptr)                                                                              \
                return CW_MANAGED_STATUS_INVALID_ARGUMENT;                                                                           \
            WriteMatrix(expression, *result);                                                                                        \
            return CW_MANAGED_STATUS_OK;                                                                                              \
        });                                                                                                                          \
    }
        CW_MATRIX_OPERATION(MathMatrixInverse, glm::inverse(ToMatrix(*matrix)))
        CW_MATRIX_OPERATION(MathMatrixAffineInverse, glm::affineInverse(ToMatrix(*matrix)))
#undef CW_MATRIX_OPERATION

        cw_managed_status CW_MANAGED_CALL MathLookAt(void* context, const cw_managed_vec3* from, const cw_managed_vec3* to,
                                                      const cw_managed_vec3* up, cw_managed_mat4* result)
        {
            return Execute(context, [&]() {
                if (from == nullptr || to == nullptr || up == nullptr || result == nullptr)
                    return CW_MANAGED_STATUS_INVALID_ARGUMENT;
                WriteMatrix(glm::lookAt(glm::vec3(from->x, from->y, from->z), glm::vec3(to->x, to->y, to->z),
                                        glm::vec3(up->x, up->y, up->z)),
                            *result);
                return CW_MANAGED_STATUS_OK;
            });
        }
    } // namespace

    void PopulateManagedHostBindings(cw_managed_host_api& api)
    {
#define CW_ASSIGN_HOST_FUNCTION(functionName, fieldName) api.fieldName = &functionName;
        CW_MANAGED_HOST_FUNCTION_LIST(CW_ASSIGN_HOST_FUNCTION)
#undef CW_ASSIGN_HOST_FUNCTION
    }
} // namespace Crowny
